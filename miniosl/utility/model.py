"""model operation --- supervised training, validation, and exporting"""
import miniosl
import miniosl.network
import numpy as np
import argparse
import coloredlogs
import logging
import torch
import json
import sys
import os

coloredlogs.install(level='INFO')
logger = logging.getLogger(__name__)

parser = argparse.ArgumentParser(description="training with extended features")
group = parser.add_mutually_exclusive_group(required=True)
group.add_argument("--train", help="npz or sfen filename for training")
group.add_argument("--validate", help="only validation", action='store_true')
group.add_argument("--export", help="filename to export the model, extension"
                   " should be .onnx/.ts (tensorrt)/.pts (torch script)")
parser.add_argument("--savefile", help="filename of output")
parser.add_argument("--loadfile", help="load filename", default='')
parser.add_argument("--device", help="torch device", default="cuda")
grp_nn = parser.add_argument_group("nn options")
grp_nn.add_argument("--n-block", help="#residual block", type=int, default=4)
grp_nn.add_argument("--n-channel", help="#channel", type=int, default=128)
grp_nn.add_argument("--ablate-bottleneck", action='store_true')
grp_l = parser.add_argument_group("training options")
grp_l.add_argument("--ablate-aux-loss", action='store_true')
grp_l.add_argument("--batch-size", type=int, default=256)
grp_l.add_argument("--epoch-limit", help="maximum epoch", type=int, default=2)
grp_l.add_argument("--step-limit", help="maximum #update, 0 for inf",
                   type=int, default=100000)
grp_l.add_argument("--report-interval", type=int, default=200)
grp_l.add_argument("--validation-interval", type=int, default=1000)
parser.add_argument("--validate-data", help="file for validation", default="")
parser.add_argument("--validation-size", type=int, help="#batch", default=100)
args = parser.parse_args()

if args.train and not args.savefile:
    args.savefile = (f'model-blk{args.n_block}-ch{args.n_channel}'
                     + f'{"-nobtl" if args.ablate_bottleneck else ""}'
                     + f'{"-noaux" if args.ablate_aux_loss else ""}')

feature_channels = len(miniosl.channel_id)
loader_class = torch.utils.data.DataLoader
deterministic_mode = "MINIOSL_DETERMINISTIC" in os.environ


def np_mse(a, b):
    return ((a-b)**2).mean()


def validate(model, loader, cfg):
    import torch.nn.functional as F
    if loader is None:
        return

    if isinstance(model, torch.nn.Module):
        model = miniosl.inference.TorchInfer(model, cfg.device)

    running_vloss_move, running_vloss_value, running_vloss_aux = 0, 0, 0
    count = 0
    top1 = 0
    with torch.no_grad():
        for i, data in enumerate(loader):
            inputs, move_labels, value_labels, aux_labels = data
            inputs = inputs.to(cfg.device)
            output_move, output_value, output_aux = model.infer(inputs)
            top1 += (np.sum(np.argmax(output_move, axis=1)
                            == move_labels.to('cpu').numpy())
                     / len(move_labels)).item()
            move_loss = F.cross_entropy(torch.from_numpy(output_move), move_labels).item()
            value_loss = np_mse(output_value.flatten(), value_labels.numpy())
            aux_loss = np_mse(output_aux, aux_labels.numpy().reshape((-1, 972)))
            running_vloss_move += move_loss
            running_vloss_value += value_loss
            running_vloss_aux += aux_loss
            count += 1
            if cfg.validation_size > 0 and i >= cfg.validation_size:
                break
    logging.info(f'validation loss: {running_vloss_move / count:.3f}, ' +
                 f'  value: {running_vloss_value / count:.3f}' +
                 f'  aux: {running_vloss_aux / count * 81 / 2:5.3f}' +
                 f'  top1: {top1 / count:.3f}')

    return running_vloss_move, running_vloss_value, running_vloss_aux


def at_the_end_of_interval(i, interval):
    return i % interval == interval - 1


def train(model, cfg):
    from torch.utils.tensorboard import SummaryWriter
    logging.info('start training')
    with open(f'{cfg.savefile}.json', 'w') as file:
        json.dump(cfg.__dict__, file)
    logging.info(f'loading {args.train}')
    dataset = miniosl.load_torch_dataset(args.train)

    criterion = torch.nn.CrossEntropyLoss()
    criterion_value = torch.nn.MSELoss()
    criterion_aux = torch.nn.MSELoss()
    optimizer = torch.optim.Adam(model.parameters(), weight_decay=1e-4)

    steps_per_epoch = (len(dataset)+cfg.batch_size-1) // cfg.batch_size
    trainloader = loader_class(dataset, batch_size=cfg.batch_size,
                               shuffle=True, num_workers=0)
    v_dataset, validate_loader = None, None
    if args.validate_data:
        logging.info(f'loading {args.validate_data} for validation')
        v_dataset = miniosl.load_torch_dataset(args.validate_data)
        validate_loader = loader_class(v_dataset, batch_size=cfg.batch_size,
                                       shuffle=not deterministic_mode, num_workers=0)
    writer = SummaryWriter()
    vsize = cfg.validation_size

    for epoch in range(cfg.epoch_limit):
        logging.info(f'start epoch {epoch}')
        if validate_loader:
            vloss = validate(model, validate_loader, args)
            writer.add_scalars('Validation',
                               {'move': vloss[0]/vsize, 'value': vloss[1]/vsize,
                                'aux': vloss[2]*81*2/vsize},
                               epoch*steps_per_epoch)
        running_loss_move, running_loss_value, running_loss_aux = 0.0, 0.0, 0.0
        writer.flush()
        for i, data in enumerate(trainloader, 0):
            model.train(True)
            inputs, move_labels, value_labels, aux_labels = [_.to(cfg.device) for _ in data]
            optimizer.zero_grad()

            outputs_move, outputs_value, outputs_aux = model(inputs)
            move_loss = criterion(outputs_move, move_labels)
            value_loss = criterion_value(outputs_value.flatten(), value_labels)
            aux_loss = criterion_aux(outputs_aux, aux_labels.reshape((-1, 972)))
            loss = move_loss + value_loss
            if not args.ablate_aux_loss:
                loss += aux_loss
            loss.backward()
            optimizer.step()

            running_loss_move += move_loss.item()
            running_loss_value += value_loss.item()
            running_loss_aux += aux_loss.item()
            if at_the_end_of_interval(i, cfg.report_interval):
                cnt = cfg.report_interval
                logging.info(f'[{epoch + 1}, {i + 1:5d}] ' +
                             f'move: {running_loss_move / cnt:5.3f}' +
                             f'  value: {running_loss_value / cnt:5.3f}'
                             f'  aux: {running_loss_aux / cnt * 81 / 2:5.3f}')
                writer.add_scalars('Train',
                                   {'move': running_loss_move/cnt,
                                    'value': running_loss_value/cnt,
                                    'aux': running_loss_aux*81*2/cnt},
                                   epoch*steps_per_epoch + i)
                running_loss_move, running_loss_value, running_loss_aux = 0.0, 0.0, 0.0
            if at_the_end_of_interval(i, cfg.validation_interval):
                if validate_loader:
                    vloss = validate(model, validate_loader, args)
                    writer.add_scalars('Validation',
                                       {'move': vloss[0]/vsize, 'value': vloss[1]/vsize,
                                        'aux': vloss[2]*81*2/vsize},
                                       epoch*steps_per_epoch + i)
                torch.save(model.state_dict(), f'interim-{args.savefile}.pt')
            if cfg.step_limit > 0 and i >= cfg.step_limit:
                logging.warning('reach step_limit')
                break
        if epoch + 1 < cfg.epoch_limit:
            torch.save(model.state_dict(), f'{args.savefile}-{epoch}.pt')

    logging.info('finish')
    torch.save(model.state_dict(), f'{args.savefile}.pt')


def main():
    network_cfg = {'in_channels': len(miniosl.channel_id),
                   'channels': args.n_channel, 'out_channels': 27,
                   'auxout_channels': 12, 'num_blocks': args.n_block,
                   'make_bottleneck': not args.ablate_bottleneck}
    model = miniosl.StandardNetwork(**network_cfg).to(args.device)
    if args.loadfile:
        logging.info(f'load {args.loadfile}')
        model = miniosl.inference.load(args.loadfile, args.device, network_cfg)
        if not args.validate:
            if not isinstance(model, miniosl.inference.TorchInfer):
                raise ValueError(f'{type(model)} is not trainable/exportable')
            model = model.model

    if args.train:
        train(model, args)
    else:
        if not args.loadfile:
            parser.print_help(sys.stderr)
            raise ValueError('need to load model for validation or export')

        if args.export:
            miniosl.export_model(model, device=args.device,
                                 filename=args.export)
        elif args.validate:
            if not args.validate_data:
                raise ValueError('validate_data not specified')
            v_dataset = miniosl.load_torch_dataset(args.validate_data)
            v_loader = loader_class(v_dataset, batch_size=args.batch_size,
                                    shuffle=not deterministic_mode,
                                    num_workers=0)
            logging.info(f'validation with {args.validate_data}')
            validate(model, v_loader, args)


if __name__ == "__main__":
    main()

# Local Variables:
# python-indent-offset: 4
# End:
