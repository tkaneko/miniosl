"""model operation --- supervised training, validation, and exporting"""
import miniosl
import miniosl.network
import numpy as np
import argparse
import logging
import torch
import json
import sys
import os
import os.path
import csv
import datetime
import tqdm
from collections import namedtuple

loader_class = torch.utils.data.DataLoader
deterministic_mode = "MINIOSL_DETERMINISTIC" in os.environ


def np_mse(a, b):
    return ((a-b)**2).mean()


def unpack_moves(u8array):
    shape = u8array.shape
    x = u8array.unsqueeze(-1).expand((-1, -1, 8))
    x = x.bitwise_right_shift(torch.arange(8).to(x.device))
    x.bitwise_and_(torch.ones_like(x))
    return x.reshape((shape[0], -1))[:, :miniosl.policy_unit]


ValidationLoss = namedtuple(
    'ValidationLoss', (
        'move', 'value', 'aux',
        'top1', 'top4', 'top8',
        'td1', 'td1p',
        'entropy4', 'entropy'
     ))


def validate(model, loader, cfg, *, pbar=None, name=None, main=False):
    import torch.nn.functional as F
    if loader is None:
        return

    if isinstance(model, torch.nn.Module):
        model = miniosl.inference.TorchInfer(model, cfg.device)

    running_vloss_move, running_vloss_value, running_vloss_aux = 0, 0, 0
    running_vloss_td1, running_vloss2_td1 = 0.0, 0.0
    running_entropy4, running_ventropy = 0.0, 0.0
    count = 0
    top8_mean = np.zeros(8)
    with torch.no_grad():
        with tqdm.tqdm(total=min(cfg.validation_size, len(loader)),
                       colour='#208dc3',
                       disable=not main) as vpbar:
            for i, data in enumerate(loader):
                inputs, move_labels, value_labels, aux_labels, \
                    inputs2, legal_moves = data
                out_move, out_value, out_aux \
                    = model.infer_int8(inputs)
                move_loss = F.cross_entropy(torch.from_numpy(out_move),
                                            move_labels.long()).item()
                value_loss = np_mse(out_value[:, 0].flatten(),
                                    value_labels.numpy())
                aux_loss = np_mse(out_aux,
                                  aux_labels.float().numpy().reshape(
                                      (-1, miniosl.aux_unit)) / miniosl.One)
                _, succ_value, *_ = model.infer_int8(inputs2)
                td1_loss = np_mse(out_value[:, 1].flatten(),
                                  -succ_value[:, 0].flatten())
                td1_loss2 = np_mse(out_value[:, 3].flatten(),
                                   -succ_value[:, 1].flatten())
                out_move = torch.from_numpy(out_move)
                top8 = out_move.topk(k=8, dim=1, sorted=True)
                top8_tensor = (top8[1] == move_labels.unsqueeze(-1))
                top8_tensor = top8_tensor.float().mean(dim=0)
                top4 = top8[0][:, :4]
                m = torch.distributions.Categorical(logits=top4)
                entropy4 = m.entropy()  # nat

                legals = unpack_moves(legal_moves).bool()
                out_move[~legals] = torch.finfo(torch.float16).min
                m = torch.distributions.Categorical(
                    logits=out_move
                )
                entropy = m.entropy().mean()  # nat
                running_ventropy += entropy.item()

                running_vloss_move += move_loss
                running_vloss_value += value_loss
                running_vloss_aux += aux_loss
                running_vloss_td1 += td1_loss
                running_vloss2_td1 += td1_loss2
                running_entropy4 += entropy4.mean().item()
                top8_mean += top8_tensor.numpy()
                count += 1
                if cfg.validation_size > 0 and i + 1 >= cfg.validation_size:
                    break
                vpbar.update(1)
    running_vloss_move /= count
    running_vloss_value /= count
    running_vloss_aux = running_vloss_aux * 81 / 2 / count
    running_vloss_td1 /= count
    running_vloss2_td1 /= count
    running_entropy4 /= count
    running_ventropy /= count
    top8_mean /= count
    if pbar:
        info = {
            'v': running_vloss_value,
            't': f'{top8_mean[0]*100:.1f}',
            'd%': f'{running_vloss_td1*100:.1f}',
            'd2%': f'{running_vloss2_td1*100:.1f}',
            'e': f'{running_ventropy:.1f}',
        }
        if main:
            info['m'] = running_vloss_move,
            info['step'] = 'v'
            info['x'] = running_vloss_aux
        pbar.set_postfix(ordered_dict=info)
    else:
        logging.info(f'validation move: {running_vloss_move:.3f},'
                     f' value: {running_vloss_value:.3f}'
                     f' ent: {running_ventropy:.3f}'
                     f' aux: {running_vloss_aux:5.3f}'
                     f' top1: {top8_mean[0]:.3f}'
                     f' top4: {top8_mean[0:3].sum():.3f}'
                     f' top8: {top8_mean[0:7].sum():.3f}'
                     f' td1: {running_vloss_td1:.3f}'
                     f' td1\': {running_vloss2_td1:.3f}'
                     )
    with open(cfg.validation_csv, 'a') as csv_output:
        csv_writer = csv.writer(csv_output, quoting=csv.QUOTE_NONNUMERIC)
        now = datetime.datetime.now().isoformat(timespec='seconds')
        if not name:
            name = cfg.loadfile
        csv_writer.writerow([now,
                             running_vloss_move, running_vloss_value,
                             running_vloss_aux,
                             top8_mean[0],
                             top8_mean[:3].sum(),
                             top8_mean[:7].sum(),
                             running_vloss_td1,
                             running_entropy4,
                             name])

    return ValidationLoss(
        move=running_vloss_move,
        value=running_vloss_value,
        aux=running_vloss_aux,
        top1=top8_mean[0],
        top4=top8_mean[:3].sum(),
        top8=top8_mean[:7].sum(),
        td1=running_vloss_td1,
        td1p=running_vloss2_td1,
        entropy4=running_entropy4,
        entropy=running_ventropy
    )


def at_the_end_of_interval(i, interval):
    return i % interval == interval - 1


def ddp_setup(rank, world_size):
    torch.cuda.set_device(rank)
    torch.distributed.init_process_group("nccl",
                                         rank=rank, world_size=world_size)


def train(rank, world_size, model, cfg, loader_class, deterministic_mode):
    device_is_cuda = cfg.device.startswith('cuda')
    """training.  may be spawned by multiprocessing if world_size > 0"""
    import torch
    torch.set_float32_matmul_precision('high')
    # 2.0
    import torch._dynamo
    if hasattr(torch._dynamo.config, 'log_level'):
        torch._dynamo.config.log_level = logging.WARNING
    from torch.utils.tensorboard import SummaryWriter
    primary = True
    if world_size > 0:
        from torch.nn.parallel import DistributedDataParallel as DDP
        ddp_setup(rank, world_size)
        primary = rank == 0
    if primary:
        logging.info('start training')
        with open(f'{cfg.savefile}.json', 'w') as file:
            json.dump(cfg.__dict__, file)
        msg = [os.path.basename(_) for _ in cfg.train] \
            if isinstance(cfg.train, list) else cfg.train
        logging.info(f'loading {msg}')
    dataset = miniosl.load_torch_dataset(cfg.train)

    criterion = torch.nn.CrossEntropyLoss()
    criterion_value = torch.nn.MSELoss()
    criterion_aux = torch.nn.MSELoss()
    optimizer = torch.optim.AdamW(model.parameters(), weight_decay=1e-4)

    steps_per_epoch = (len(dataset)+cfg.batch_size-1) // cfg.batch_size
    shuffle, sampler, device = True, None, cfg.device
    batch_size = cfg.batch_size
    if world_size > 0:
        device = f'cuda:{rank}'
        sampler = torch.utils.data.distributed.DistributedSampler(dataset)
        shuffle = False
        batch_size //= world_size
    trainloader = loader_class(
        dataset, batch_size=batch_size,
        collate_fn=lambda indices: dataset.collate(indices),
        shuffle=shuffle, sampler=sampler, num_workers=0)
    v_dataset, validate_loader = None, None
    if cfg.validate_data:
        logging.info(f'loading {cfg.validate_data} for validation')
        v_dataset = miniosl.load_torch_dataset(cfg.validate_data)
        if world_size > 0:
            sampler = torch.utils.data.distributed.DistributedSampler(
                v_dataset)
        validate_loader = loader_class(
            v_dataset, batch_size=batch_size,
            shuffle=not deterministic_mode and shuffle,
            collate_fn=lambda indices: v_dataset.collate(indices),
            sampler=sampler,
            num_workers=0)
    writer = SummaryWriter(comment='SL')

    if world_size > 0:
        model = DDP(model.to(device), device_ids=[rank])
    compiled_model = torch.compile(model)

    bar_format = "{l_bar}{bar}"\
        "| {n_fmt}/{total_fmt} [{elapsed}<{remaining}{postfix}]"
    pbar_interval = max(1, 4096 // batch_size)
    for epoch in range(cfg.epoch_limit):
        if world_size > 0:
            trainloader.sampler.set_epoch(epoch)
        if primary:
            logging.info(f'start epoch {epoch}')
        scaler = torch.cuda.amp.GradScaler(enabled=device_is_cuda)
        with tqdm.tqdm(total=min(cfg.step_limit, steps_per_epoch),
                       smoothing=0.7, bar_format=bar_format,
                       colour='#208dc3',
                       disable=not primary) as pbar:
            if validate_loader:
                vname = f'{cfg.savefile}-{epoch}-0'
                vloss = validate(model, validate_loader,
                                 cfg, pbar=pbar, name=vname)
                if primary:
                    writer.add_scalars('Validation', {
                        'move': vloss.move, 'value': vloss.value,
                        'aux': vloss.aux, 'top1': vloss.top1,
                        'td1': vloss.td1, 'entropy': vloss.entropy
                    }, epoch*steps_per_epoch)
            running_loss_move, running_loss_value, running_loss_aux \
                = 0.0, 0.0, 0.0
            running_loss_td1, running_loss_soft = 0.0, 0.0
            running_loss2_td1 = 0.0
            running_entropy = 0.0
            writer.flush()
            for i, data in enumerate(trainloader, 0):
                if i >= steps_per_epoch:
                    break
                if primary:
                    if (i+1) % pbar_interval == 0:
                        pbar.update(pbar_interval)
                    elif not device_is_cuda:
                        pbar.update(1)
                compiled_model.train(True)
                inputs, move_labels, value_labels, aux_labels, \
                    inputs2, legal_moves = [_.to(device) for _ in data]
                inputs, aux_labels, inputs2 \
                    = inputs.float(), aux_labels.float(), inputs2.float()
                inputs /= miniosl.One
                move_labels = move_labels.long()
                aux_labels /= miniosl.One
                inputs2 /= miniosl.One
                if cfg.policy_commit < 1.0:
                    dist_moves = unpack_moves(legal_moves).float()
                    n_moves = dist_moves.sum(dim=-1)
                    small_prob = ((1 - cfg.policy_commit)
                                  / n_moves.unsqueeze(-1))
                    dist_moves *= small_prob
                    # defer setting the probability for move_labels

                optimizer.zero_grad()
                with torch.autocast(device_type="cuda",
                                    enabled=device_is_cuda):
                    out_move, out_value, out_aux \
                        = compiled_model(inputs)
                    if cfg.topk > 1:
                        target_logit = out_move.gather(
                            dim=1,
                            index=move_labels.unsqueeze(-1)
                        )
                        _, topk_idx = out_move.detach().topk(k=cfg.topk, dim=1)
                        out_move = out_move.scatter(
                            dim=1,
                            index=topk_idx,
                            value=torch.finfo(torch.float16).min/8
                        )
                        out_move = out_move.scatter(
                            dim=1,
                            index=move_labels.unsqueeze(-1),
                            src=target_logit
                        )
                        if cfg.policy_commit < 1.0:
                            dist_moves.scatter_(
                                dim=1, index=topk_idx, value=0
                            )
                    if cfg.policy_commit < 1.0:
                        dist_moves.scatter_(
                            dim=1, index=move_labels.unsqueeze(-1),
                            src=(cfg.policy_commit + small_prob)
                        )

                    if cfg.policy_commit == 1.0:
                        move_loss = criterion(out_move, move_labels)
                    else:
                        move_loss = criterion(out_move, dist_moves)
                    value_loss = criterion_value(out_value[:, 0].flatten(),
                                                 value_labels)
                    aux_loss = criterion_aux(
                        out_aux,
                        aux_labels.reshape((-1, miniosl.aux_unit)))

                    if cfg.entropy_bonus > 0:
                        legals = unpack_moves(legal_moves).bool()
                        out_move[~legals] = torch.finfo(torch.float16).min
                        m = torch.distributions.Categorical(
                            logits=out_move
                        )
                        entropy = m.entropy().mean()  # nat
                        running_entropy += entropy.item()

                    with torch.no_grad():
                        _, succ_value, *_ = compiled_model(inputs2)
                        tdtarget = -succ_value[:, 0].flatten().detach()
                        tdtarget2 = -succ_value[:, 1].flatten().detach()
                        softtarget = -succ_value[:, 2].flatten().detach()
                        logp = torch.nn.functional.log_softmax(
                            out_move, dim=-1)
                        logp = logp.gather(1, move_labels.unsqueeze(-1)
                                           ).squeeze(-1)
                        logp = torch.minimum(-logp,
                                             torch.ones_like(logp)*4) / 4
                        softtarget += -logp
                        softtarget = torch.minimum(softtarget,
                                                   torch.ones_like(softtarget))

                    td1_loss = criterion_value(
                        out_value[:, 1].flatten(), tdtarget)
                    soft_loss = criterion_value(
                        out_value[:, 2].flatten(), softtarget)
                    td1_loss2 = criterion_value(
                        out_value[:, 3].flatten(), tdtarget2)

                    loss = move_loss + value_loss
                    if cfg.entropy_bonus > 0:
                        loss -= cfg.entropy_bonus * entropy

                    if not cfg.ablate_aux_loss:
                        loss += aux_loss * 0.1
                        loss += td1_loss
                        loss += td1_loss2
                        loss += soft_loss
                scaler.scale(loss).backward()
                scaler.step(optimizer)
                scaler.update()

                running_loss_move += move_loss.item()
                running_loss_value += value_loss.item()
                running_loss_aux += aux_loss.item()
                running_loss_td1 += td1_loss.item()
                running_loss2_td1 += td1_loss2.item()
                running_loss_soft += soft_loss.item()
                if at_the_end_of_interval(i, cfg.report_interval) and primary:
                    cnt = cfg.report_interval
                    pbar.set_postfix(ordered_dict={
                        'step': i+1, 'm': running_loss_move / cnt,
                        'v': running_loss_value / cnt,
                        'x': running_loss_aux / cnt * 81 / 2,
                        '2%': f'{running_loss_td1 / cnt * 100:.1f}',
                        '4%': f'{running_loss2_td1 / cnt * 100:.1f}',
                        's': running_loss_soft / cnt,
                    })
                    report_data = {'move': running_loss_move/cnt,
                                   'value': running_loss_value/cnt,
                                   'aux': running_loss_aux*81/2/cnt,
                                   'td1': running_loss_td1/cnt,
                                   'soft': running_loss_soft/cnt,
                                   }
                    if cfg.entropy_bonus > 0:
                        report_data.update({'entropy': running_entropy/cnt})

                    writer.add_scalars('Train',
                                       report_data,
                                       epoch*steps_per_epoch + i)
                    running_loss_move, running_loss_value, running_loss_aux \
                        = 0.0, 0.0, 0.0
                    running_loss_td1, running_loss_soft = 0.0, 0.0
                    running_loss2_td1 = 0.0
                    running_entropy = 0.0
                if at_the_end_of_interval(i, cfg.validation_interval):
                    if validate_loader:
                        vname = f'{cfg.savefile}-{epoch}-{i+1}'
                        vloss = validate(model, validate_loader, cfg,
                                         pbar=pbar, name=vname)
                        if primary:
                            writer.add_scalars('Validation', {
                                'move': vloss.move, 'value': vloss.value,
                                'aux': vloss.aux, 'top1': vloss.top1,
                                'td1': vloss.td1,
                                'entropy': vloss.entropy,
                            }, epoch*steps_per_epoch + i)
                    learned = model.state_dict() \
                        if world_size == 0 \
                        else model.module.state_dict()
                    torch.save(learned, f'interim-{cfg.savefile}.pt')
                if cfg.step_limit > 0 and i >= cfg.step_limit:
                    if primary:
                        logging.warning('reach step_limit')
                    break
        if epoch + 1 < cfg.epoch_limit and primary:
            learned = model.state_dict() if world_size == 0 \
                else model.module.state_dict()
            torch.save(learned, f'{cfg.savefile}-{epoch}.pt')
    if primary:
        logging.info('finish')
        learned = model.state_dict() \
            if world_size == 0 \
            else model.module.state_dict()
        torch.save(learned, f'{cfg.savefile}.pt')
    if world_size > 0:
        torch.distributed.destroy_process_group()


def main():
    os.environ['TF_CPP_MIN_LOG_LEVEL'] = '2'
    import torch._dynamo
    if hasattr(torch._dynamo.config, 'log_level'):
        torch._dynamo.config.log_level = logging.WARNING
    parser = argparse.ArgumentParser(
        description="training with extended features",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--train", nargs='*',
                       help="npz or sfen filename for training")
    group.add_argument("--validate", help="only validation",
                       action='store_true')
    group.add_argument("--export",
                       help="filename to export the model, extension"
                       " should be .onnx/.ts (tensorrt)/.pts (torch script)")
    parser.add_argument("--savefile", help="filename of output")
    parser.add_argument("--loadfile", help="load filename", default='')
    parser.add_argument("--no-load-strict", action='store_true')
    parser.add_argument("--device", help="torch device", default="cuda:0")
    grp_nn = parser.add_argument_group("nn options")
    grp_nn.add_argument("--n-block", help="#residual block",
                        type=int, default=4)
    grp_nn.add_argument("--n-channel", help="#channel", type=int, default=128)
    grp_nn.add_argument("--broadcast-every",
                        help="insert broadcast periodically",
                        type=int, default=4)
    grp_nn.add_argument("--bn-momentum",
                        help="momentum in BatchNorm", type=float,
                        default=0.1)
    grp_nn.add_argument("--compiled", action='store_true')
    grp_nn.add_argument("--remove-aux-head", action='store_true')
    grp_l = parser.add_argument_group("training options")
    grp_l.add_argument("--policy-commit", type=float, default=1.0)  # smoothing in CE
    grp_l.add_argument("--entropy-bonus", type=float, default=0.0)
    grp_l.add_argument("--topk", type=int, default=1)
    grp_l.add_argument("--ablate-aux-loss", action='store_true')
    grp_l.add_argument("--batch-size", type=int, default=1024)
    grp_l.add_argument("--epoch-limit", help="maximum epoch",
                       type=int, default=2)
    grp_l.add_argument("--step-limit", help="maximum #update, 0 for inf",
                       type=int, default=100000)
    grp_l.add_argument("--report-interval", type=int, default=200)
    grp_l.add_argument("--validation-interval", type=int, default=1000)
    parser.add_argument("--validate-data",
                        help="file for validation", default="")
    parser.add_argument("--validation-csv",
                        help="file to append validation result",
                        default="validation.csv")
    parser.add_argument("--validation-size", type=int,
                        help="#batch", default=100)
    parser.add_argument("--quiet", action='store_true')

    if deterministic_mode:
        torch.manual_seed(0)
        np.random.seed(0)
        # random.seed(0)

    args = parser.parse_args()
    miniosl.install_coloredlogs(level=('ERROR' if args.quiet else 'INFO'))
    logger = logging.getLogger(__name__)

    if args.validate_data and not os.path.exists(args.validate_data):
        raise ValueError(f'file not found {args.validate_data}')

    if args.train and not args.savefile:
        args.savefile = (f'model-blk{args.n_block}-ch{args.n_channel}'
                         + f'{"-noaux" if args.ablate_aux_loss else ""}')

    network_cfg = {'in_channels': len(miniosl.channel_id),
                   'channels': args.n_channel, 'out_channels': 27,
                   'auxout_channels': miniosl.aux_unit//81,
                   'num_blocks': args.n_block,
                   'bn_momentum': args.bn_momentum,
                   'broadcast_every': args.broadcast_every}
    model = miniosl.StandardNetwork(**network_cfg).to(args.device)
    if args.loadfile:
        logging.info(f'load {args.loadfile}')
        model = miniosl.inference.load(args.loadfile, args.device, network_cfg,
                                       compiled=args.compiled,
                                       strict=not args.no_load_strict,
                                       remove_aux_head=args.remove_aux_head)
        if not args.validate:
            if not isinstance(model, miniosl.inference.TorchInfer):
                raise ValueError(f'{type(model)} is not trainable/exportable')
            model = model.model

    if args.train:
        params = sum(p.numel() for p in model.parameters() if p.requires_grad)
        logging.info(f'#trainable parameters = {params}')
        if args.device.startswith('cuda') and torch.cuda.device_count() > 1 \
           and not deterministic_mode:
            world_size = torch.cuda.device_count()
            port = 29500
            os.environ["MASTER_ADDR"] = "localhost"
            os.environ["MASTER_PORT"] = str(29500)
            logger.info(f'run ddp {world_size} port {port}')
            torch.multiprocessing.spawn(
                train,
                args=(world_size, model, args, loader_class, False),
                nprocs=world_size,
                join=True)
        else:
            train(0, 0, model, args, loader_class, deterministic_mode)
    else:
        if not args.loadfile:
            parser.print_help(sys.stderr)
            raise ValueError('need to load model for validation or export')

        if args.export:
            miniosl.export_model(model, device=args.device,
                                 filename=args.export,
                                 quiet=args.quiet,
                                 remove_aux_head=args.remove_aux_head)
        elif args.validate:
            if not args.validate_data:
                raise ValueError('validate_data not specified')
            v_dataset = miniosl.load_torch_dataset(args.validate_data)
            v_loader = loader_class(
                v_dataset, batch_size=args.batch_size,
                shuffle=not deterministic_mode,
                collate_fn=lambda indices: v_dataset.collate(indices),
                num_workers=0)
            logging.info(f'validation with {args.validate_data},'
                         f' concurrency {miniosl.parallel_threads()}')
            ret = validate(model, v_loader, args, main=True)
            logging.debug(f'{ret=}')


if __name__ == "__main__":
    main()

# Local Variables:
# python-indent-offset: 4
# End:
