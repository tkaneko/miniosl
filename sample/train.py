import miniosl
import miniosl.network
import argparse
import logging
import torch
import numpy as np
import json
import random
import sys


FORMAT = '%(asctime)s %(levelname)s %(message)s'
logging.basicConfig(format=FORMAT, level=logging.DEBUG, force=True)

parser = argparse.ArgumentParser(description="training with extended features")
group = parser.add_mutually_exclusive_group(required=True)
group.add_argument("--train-npz", help="npz filename for training")
group.add_argument("--inspect", help="visualize instance", action='store_true')
group.add_argument("--export-onnx", help="export onnx", action='store_true')
group.add_argument("--export-tensorrt", action='store_true')
group.add_argument("--export-torch-script", action='store_true')
parser.add_argument("--half-precision", action='store_true')
parser.add_argument("--savefile", help="filename of output", default='out')
parser.add_argument("--loadfile", help="load filename", default='')
parser.add_argument("--device", help="torch device", default="cuda")
grp_nn = parser.add_argument_group("nn options")
grp_nn.add_argument("--n-block", help="#residual block", type=int, default=4)
grp_nn.add_argument("--n-channel", help="#channel", type=int, default=128)
grp_nn.add_argument("--ablate-bottleneck", action='store_true')
grp_l = parser.add_argument_group("training options")
grp_l.add_argument("--ablate-aux-loss", action='store_true')
grp_l.add_argument("--batch-size", type=int, default=4096)
grp_l.add_argument("--epoch-limit", help="maximum epoch", type=int, default=2)
grp_l.add_argument("--step-limit", help="maximum #updates, 0 for inf",
                   type=int, default=100000)
grp_l.add_argument("--report-interval", type=int, default=200)
grp_l.add_argument("--validation-interval", type=int, default=1000)
parser.add_argument("--validate-npz", help="file for validation", default="")
parser.add_argument("--validation-size", help="#batches", default=100)
args = parser.parse_args()

if args.train_npz and args.savefile == 'out':
    args.savefile = (f'model-blk{args.n_block}-ch{args.n_channel}'
                     + f'{"-nobtl" if args.ablate_bottleneck else ""}'
                     + f'{"-noaux" if args.ablate_aux_loss else ""}')

feature_channels = len(miniosl.channel_id)


def validate(model, loader, cfg):
    import torch.nn.functional as F
    if loader is None:
        return

    model.eval()
    running_vloss_move, running_vloss_value, running_vloss_aux = 0, 0, 0
    count = 0
    top1 = 0
    with torch.no_grad():
        for i, data in enumerate(loader):
            inputs, move_labels, value_labels, aux_labels = [_.to(cfg.device) for _ in data]
            output_move, output_value, output_aux = model(inputs)

            top1 += (torch.sum(torch.argmax(output_move, dim=1) == move_labels)
                     / len(move_labels)).item()
            move_loss = F.cross_entropy(output_move, move_labels).item()
            value_loss = F.mse_loss(output_value.flatten(), value_labels).item()
            aux_loss = F.mse_loss(output_aux, aux_labels).item()
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
    logging.info('start training')
    dataset = miniosl.StandardDataset(args.train_npz)
    v_dataset = miniosl.StandardDataset(args.validate_npz) if args.validate_npz else None

    criterion = torch.nn.CrossEntropyLoss()
    criterion_value = torch.nn.MSELoss()
    criterion_aux = torch.nn.MSELoss()
    optimizer = torch.optim.Adam(model.parameters(), weight_decay=1e-4)

    loader_class = torch.utils.data.DataLoader
    trainloader = loader_class(dataset, batch_size=cfg.batch_size,
                               shuffle=True, num_workers=0)
    validate_loader = None if v_dataset is None \
        else loader_class(v_dataset, batch_size=cfg.batch_size, shuffle=True,
                          num_workers=0)

    for epoch in range(cfg.epoch_limit):
        logging.info(f'start epoch {epoch}')
        validate(model, validate_loader, args)
        running_loss_move, running_loss_value, running_loss_aux = 0.0, 0.0, 0.0
        for i, data in enumerate(trainloader, 0):
            model.train(True)
            inputs, move_labels, value_labels, aux_labels = [_.to(cfg.device) for _ in data]
            optimizer.zero_grad()

            outputs_move, outputs_value, outputs_aux = model(inputs)
            move_loss = criterion(outputs_move, move_labels)
            value_loss = criterion_value(outputs_value.flatten(), value_labels)
            aux_loss = criterion_aux(outputs_aux, aux_labels)
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
                running_loss_move, running_loss_value, running_loss_aux = 0.0, 0.0, 0.0
            if at_the_end_of_interval(i, cfg.validation_interval):
                validate(model, validate_loader, args)
                torch.save(model.state_dict(), f'interim-{args.savefile}.pt')
            if cfg.step_limit > 0 and i >= cfg.step_limit:
                logging.warning('reach step_limit')
                break
        if epoch + 1 < cfg.epoch_limit:
            torch.save(model.state_dict(), f'{args.savefile}-{epoch}.pt')

    logging.info('finish')
    torch.save(model.state_dict(), f'{args.savefile}.pt')


def visualize_instance(model, dataset, cfg):
    id = random.randrange(len(dataset))
    input, move_label, value_label, aux_label = dataset[id]
    model.eval()

    model_out = model(torch.stack((input,)).to(cfg.device))
    move_logit, value_out, aux_out = [_.detach().to('cpu') for _ in model_out]
    out_flatten = torch.softmax(move_logit[0], 0).numpy()

    item = dataset.raw_data320(id)
    state = item.make_state()
    move = item.base.move
    moves = miniosl.State(state).genmove()
    prob = [(out_flatten[move.policy_move_label()], move) for move in moves]
    prob.sort(key=lambda e: -e[0])

    outfile = f'{args.savefile}-{id}.txt'
    with open(outfile, 'w') as file:
        print('id = ', id, file=file)
        print(move.to_csa(), file=file)
        for p, m in prob:
            if p > 0.05:
                print(f'{p:5.2f}% {m} {m.policy_move_label()}', file=file)
        print('value = ', value_out, file=file)

    state.to_png(plane=np.max(out_flatten.reshape(-1, 9, 9), axis=0),
                 filename=f'{args.savefile}-{id}.png')
    logging.info(f'wrote {outfile} and png')


def inspect(model, cfg):
    v_dataset = miniosl.dataset.StandardDataset(args.validate_npz)
    validate_loader = torch.utils.data.DataLoader(v_dataset,
                                                  batch_size=cfg.batch_size,
                                                  shuffle=True, n_workers=0)
    validate(model, validate_loader, args)
    visualize_instance(model, v_dataset, args)


def export_onnx(model, args):
    import torch.onnx
    model.eval()
    dtype = torch.float
    if args.half_precision:
        model = model.half()
        dtype = torch.half
    dummy_input = torch.randn(16, feature_channels, 9, 9, device=args.device,
                              dtype=dtype)
    filename = args.savefile if args.savefile.endswith('.onnx') else f'{args.savefile}.onnx'
    torch.onnx.export(model, dummy_input, filename,
                      dynamic_axes={'input': {0: 'batch_size'},
                                    'move': {0: 'batch_size'},
                                    'aux': {0: 'batch_size'}},
                      verbose=True, input_names=['input'],
                      output_names=['move', 'value', 'aux'])


def export_tensorrt(model, args):
    import torch_tensorrt
    model.eval()
    model = model.half()
    inputs = [
        torch_tensorrt.Input(
            min_shape=[1, feature_channels, 9, 9],
            opt_shape=[128, feature_channels, 9, 9],
            max_shape=[2048, feature_channels, 9, 9],
            dtype=torch.half)]
    enabled_precisions = {torch.half}
    trt_ts_module = torch_tensorrt.compile(
        model, inputs=inputs, enabled_precisions=enabled_precisions
    )
    input_data = torch.randn(16, feature_channels, 9, 9, device=args.device)
    _ = trt_ts_module(input_data.half())
    filename = args.savefile if args.savefile.endswith('.ts') else f'{args.savefile}.ts'
    torch.jit.save(trt_ts_module, filename)


def export_torch_script(model, args):
    model.eval()
    if args.device == 'cuda':
        model = model.half()
        inputs = torch.rand(8, feature_channels, 9, 9).half()
        ts_module = torch.jit.trace(model, inputs)
    else:
        ts_module = torch.jit.script(model)

    filename = args.savefile if args.savefile.endswith('.ptts') else f'{args.savefile}.ptts'
    torch.jit.save(ts_module, filename)


if __name__ == "__main__":
    with open(f'{args.savefile}.json', 'w') as file:
        json.dump(args.__dict__, file)

    network_cfg = {'in_channels': len(miniosl.channel_id),
                   'channels': args.n_channel, 'out_channels': 27,
                   'auxout_channels': 12, 'num_blocks': args.n_block,
                   'make_bottleneck': not args.ablate_bottleneck}
    model = miniosl.StandardNetwork(**network_cfg).to(args.device)
    if args.loadfile:
        logging.info(f'load {args.loadfile}')
        saved_state = torch.load(args.loadfile,
                                 map_location=torch.device(args.device))
        model.load_state_dict(saved_state)
    elif not args.train_npz:
        parser.print_help(sys.stderr)
        raise ValueError('need to load model before export')

    if args.inspect:
        inspect(model, args)
    elif args.export_onnx:
        export_onnx(model, args)
    elif args.export_tensorrt:
        export_tensorrt(model, args)
    elif args.export_torch_script:
        export_torch_script(model, args)
    else:
        train(model, args)

# Local Variables:
# python-indent-offset: 4
# End:
