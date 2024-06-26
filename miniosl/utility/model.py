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
import recordclass


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


LossStats = recordclass.recordclass(
    'LossStats', (
        'move', 'value', 'aux', 'td1', 'td1p', 'soft',
        'entropy4', 'entropy', 'top1', 'top4', 'top8',
     ))


def make_loss_stats():
    return LossStats(*np.zeros(len(LossStats())).tolist())


def record_vloss(validation_csv, name, vloss):
    with open(validation_csv, 'a') as csv_output:
        csv_writer = csv.writer(csv_output, quoting=csv.QUOTE_NONNUMERIC)
        now = datetime.datetime.now().isoformat(timespec='seconds')
        csv_writer.writerow([
            now, vloss.move, vloss.value, vloss.aux,
            vloss.top1, vloss.top4, vloss.top8,
            vloss.td1, vloss.entropy4, name
        ])


def log_vloss(loss):
    logging.info(
        f'validation move:{loss.move:.3f} value:{loss.value:.3f}'
        f' ent:{loss.entropy:.3f} aux:{loss.aux:5.3f}'
        f' top1:{loss.top1:5.3f} top4:{loss.top4:5.3f} top8:{loss.top8:5.3f}'
        f' td1:{loss.td1:.3f} td1\':{loss.td1p:.3f}'
    )


def update_validation_pbar(pbar, vloss, main: bool):
    info = {
        'v': vloss.value, 't': f'{vloss.top1*100:.1f}',
        'd%': f'{vloss.td1*100:.1f}', 'd2%': f'{vloss.td1p*100:.1f}',
        'e': f'{vloss.entropy:.1f}',
    }
    if main:
        info['m'] = vloss.move,
        info['step'] = 'v'
        info['x'] = vloss.aux
    pbar.set_postfix(ordered_dict=info)


def send_vloss(writer, vloss, *, cumulative_step):
    writer.add_scalars('Validation', {
        'move': vloss.move, 'value': vloss.value,
        'aux': vloss.aux, 'top1': vloss.top1,
        'td1': vloss.td1, 'entropy': vloss.entropy
    }, cumulative_step)


def div_loss_stat(loss_stat, denom):
    for i in range(len(loss_stat)):
        loss_stat[i] /= denom


def add_to_loss_stat(loss, move_loss, value_loss, aux_loss,
                     td1_loss, td1_loss2,
                     *, soft=None, entropy=None, entropy4=None):
    loss.move += move_loss
    loss.value += value_loss
    loss.aux += aux_loss
    loss.td1 += td1_loss
    loss.td1p += td1_loss2
    if soft:
        loss.soft += soft
    if entropy:
        loss.entropy += entropy
    if entropy4:
        loss.entropy4 += entropy4


def report_train_loss(rloss, pbar, writer, *, step, cumulative_step):
    pbar.set_postfix(ordered_dict={
        'step': step+1, 'm': rloss.move, 'v': rloss.value,
        'x': rloss.aux * 81 / 2,
        '2%': f'{rloss.td1 * 100:.1f}', '4%': f'{rloss.td1p * 100:.1f}',
        's': rloss.soft,
    })
    report_data = {
        'move': rloss.move, 'value': rloss.value, 'aux': rloss.aux*81/2,
        'td1': rloss.td1, 'soft': rloss.soft,
    }
    writer.add_scalars('Train', report_data, cumulative_step)


def entropy_of_logits(logits: torch.Tensor):
    m = torch.distributions.Categorical(logits=logits)
    return m.entropy().mean().item()


"""input features (obs, obs_after) and labels (others)"""
BatchInput = recordclass.recordclass(
    'BatchInput', (
        'obs', 'move', 'value', 'aux',
        'obs_after',  'legal_move'))


BatchOutput = recordclass.recordclass(
    'BatchOutput', ('move', 'value', 'aux'))


def validate(model, loader, cfg, *, pbar=None, name=None, main=False):
    import torch.nn.functional as F
    if loader is None:
        return

    if isinstance(model, torch.nn.Module):
        model = miniosl.inference.TorchInfer(model, cfg.device)

    vloss = make_loss_stats()
    count = 0
    top8_mean = np.zeros(8)
    with torch.no_grad():
        with tqdm.tqdm(total=min(cfg.validation_size, len(loader)),
                       colour='#208dc3',
                       disable=not main) as vpbar:
            for i, data in enumerate(loader):
                input = BatchInput(*data)
                out = BatchOutput(*model.infer_int8(input.obs))
                move_loss = F.cross_entropy(torch.from_numpy(out.move),
                                            input.move.long()).item()
                value_loss = np_mse(out.value[:, 0].flatten(),
                                    input.value.numpy())
                aux_loss = np_mse(out.aux,
                                  input.aux.float().numpy().reshape(
                                      (-1, miniosl.aux_unit)) / miniosl.One)
                _, succ_value, *_ = model.infer_int8(input.obs_after)
                td1_loss = np_mse(out.value[:, 1].flatten(),
                                  -succ_value[:, 0].flatten())
                td1_loss2 = np_mse(out.value[:, 3].flatten(),
                                   -succ_value[:, 1].flatten())
                out.move = torch.from_numpy(out.move)
                top8 = out.move.topk(k=8, dim=1, sorted=True)
                top8_tensor = (top8[1] == input.move.unsqueeze(-1))
                top8_tensor = top8_tensor.float().mean(dim=0)
                top4 = top8[0][:, :4]
                legals = unpack_moves(input.legal_move).bool()
                out.move[~legals] = torch.finfo(torch.float16).min
                add_to_loss_stat(
                    vloss, move_loss, value_loss, aux_loss,
                    td1_loss, td1_loss2,
                    entropy=entropy_of_logits(out.move),
                    entropy4=entropy_of_logits(top4)  # nat
                )
                top8_mean += top8_tensor.numpy()
                count += 1
                if cfg.validation_size > 0 and i + 1 >= cfg.validation_size:
                    break
                vpbar.update(1)
    div_loss_stat(vloss, count)
    vloss.aux *= 81 / 2
    top8_mean /= count
    vloss.top1 = top8_mean[0]
    vloss.top4 = top8_mean[:3].sum()
    vloss.top8 = top8_mean[:7].sum()
    if pbar:
        update_validation_pbar(pbar, vloss, main)
    else:
        log_vloss(vloss)
    record_vloss(cfg.validation_csv, name or cfg.loadfile, vloss)
    return vloss


def setup_tensors(inputs: BatchInput):
    inputs.obs = inputs.obs.float()
    inputs.aux = inputs.aux.float()
    inputs.obs_after = inputs.obs_after.float()
    inputs.obs /= miniosl.One
    inputs.move = inputs.move.long()
    inputs.aux /= miniosl.One
    inputs.obs_after /= miniosl.One


def at_the_end_of_interval(i, interval):
    return i % interval == interval - 1


def ddp_setup(rank, world_size):
    torch.cuda.set_device(rank)
    torch.distributed.init_process_group("nccl",
                                         rank=rank, world_size=world_size)


def save_model(model, filename, is_single):
    learned = model.state_dict() if is_single else model.module.state_dict()
    torch.save(learned, filename)


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

    centropy = torch.nn.CrossEntropyLoss()
    mse = torch.nn.MSELoss()
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
            if validate_loader and primary:
                vname = f'{cfg.savefile}-{epoch}-0'
                vloss = validate(model, validate_loader,
                                 cfg, pbar=pbar, name=vname)
                send_vloss(writer, vloss,
                           cumulative_step=epoch*steps_per_epoch)
            rloss = make_loss_stats()
            if primary:
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
                batch = BatchInput(*[_.to(device) for _ in data])
                setup_tensors(batch)
                if cfg.policy_commit < 1.0:
                    dist_moves = unpack_moves(batch.legal_move).float()
                    n_moves = dist_moves.sum(dim=-1)
                    small_prob = ((1 - cfg.policy_commit)
                                  / n_moves.unsqueeze(-1))
                    dist_moves *= small_prob
                    # defer setting the probability for move_labels

                optimizer.zero_grad()
                with torch.autocast(device_type="cuda",
                                    enabled=device_is_cuda):
                    out = BatchOutput(*compiled_model(batch.obs))
                    move_target = batch.move
                    if cfg.policy_commit < 1.0:
                        dist_moves.scatter_(
                            dim=1, index=batch.move.unsqueeze(-1),
                            src=(cfg.policy_commit + small_prob)
                        )
                        move_target = dist_moves

                    move_loss = centropy(out.move, move_target)
                    value_loss = mse(out.value[:, 0].flatten(),
                                     batch.value)
                    aux_loss = mse(
                        out.aux, batch.aux.reshape((-1, miniosl.aux_unit))
                    )
                    with torch.no_grad():
                        _, succ_value, *_ = compiled_model(batch.obs_after)
                        tdtarget = -succ_value[:, 0].flatten().detach()
                        tdtarget2 = -succ_value[:, 1].flatten().detach()
                        softtarget = -succ_value[:, 2].flatten().detach()
                        logp = torch.nn.functional.log_softmax(out.move, dim=-1)
                        logp = logp.gather(1, batch.move.unsqueeze(-1)
                                           ).squeeze(-1)
                        logp = torch.minimum(-logp,
                                             torch.ones_like(logp)*4) / 4
                        softtarget += -logp
                        softtarget = torch.minimum(softtarget,
                                                   torch.ones_like(softtarget))

                    td1_loss = mse(out.value[:, 1].flatten(), tdtarget)
                    soft_loss = mse(out.value[:, 2].flatten(), softtarget)
                    td1_loss2 = mse(out.value[:, 3].flatten(), tdtarget2)

                    loss = move_loss + value_loss

                    if not cfg.ablate_aux_loss:
                        loss += aux_loss * 0.1 + td1_loss + td1_loss2
                        loss += soft_loss
                scaler.scale(loss).backward()
                scaler.step(optimizer)
                scaler.update()

                add_to_loss_stat(
                    rloss, move_loss.item(), value_loss.item(), aux_loss.item(),
                    td1_loss.item(), td1_loss2, soft=soft_loss.item()
                )
                if at_the_end_of_interval(i, cfg.report_interval) and primary:
                    div_loss_stat(rloss, cfg.report_interval)
                    report_train_loss(rloss, pbar, writer, step=i,
                                      cumulative_step=epoch*steps_per_epoch+i)
                    rloss = make_loss_stats()

                if primary \
                   and at_the_end_of_interval(i, cfg.validation_interval):
                    if validate_loader:
                        vname = f'{cfg.savefile}-{epoch}-{i+1}'
                        vloss = validate(model, validate_loader, cfg,
                                         pbar=pbar, name=vname)
                        send_vloss(writer, vloss,
                                   cumulative_step=epoch*steps_per_epoch+i)
                    save_model(model, f'interim-{cfg.savefile}.pt',
                               world_size == 0)
                if cfg.step_limit > 0 and i >= cfg.step_limit:
                    if primary:
                        logging.warning('reach step_limit')
                    break
        if epoch + 1 < cfg.epoch_limit and primary:
            save_model(model, f'{cfg.savefile}-{epoch}.pt', world_size == 0)
    if primary:
        logging.info('finish')
        save_model(model, f'{cfg.savefile}.pt', world_size == 0)
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
                        type=int, default=9)
    grp_nn.add_argument("--n-channel", help="#channel", type=int, default=256)
    grp_nn.add_argument("--broadcast-every",
                        help="insert broadcast periodically",
                        type=int, default=3)
    grp_nn.add_argument("--compiled", action='store_true')
    grp_nn.add_argument("--remove-aux-head", action='store_true')
    grp_l = parser.add_argument_group("training options")
    grp_l.add_argument("--policy-commit", type=float,
                       help="smoothing in cross entropy loss if <1",
                       default=127/128)
    grp_l.add_argument("--ablate-aux-loss", action='store_true')
    grp_l.add_argument("--batch-size", type=int,
                       help="batch size",
                       default=1024)
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
            if args.export:
                logger.warning('export without loading weights')
            else:
                parser.print_help(sys.stderr)
                raise ValueError('need to load model for validation')

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
