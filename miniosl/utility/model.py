"""model operation --- supervised training, validation, and exporting"""
import miniosl
import miniosl.network
import numpy as np
import click
import tqdm
import tqdm.contrib.logging
import recordclass
import torch
import logging
import collections
import os
import os.path
import csv
import datetime
import json


loader_class = torch.utils.data.DataLoader
deterministic_mode = "MINIOSL_DETERMINISTIC" in os.environ
global_config = {}


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
    return LossStats(*np.zeros(len(LossStats())))


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


ValidationCfg = collections.namedtuple(
    'ValidationCfg',
    ('device', 'validation_size', 'validation_csv')
)


def do_validate(model, loader, cfg: ValidationCfg,
                *, pbar=None, name=None, main=False):
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
    record_vloss(cfg.validation_csv, name, vloss)
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


TrainingCfg = collections.namedtuple(
    'TrainingCfg', ValidationCfg._fields + (
        'trainfiles', 'savefile',
        'batch_size', 'step_limit', 'epoch_limit', 'report_interval',
        'policy_commit', 'ablate_aux_loss',
        'validation_data', 'validation_interval',
     )
)


def do_train(rank, world_size, model, cfg: TrainingCfg, loader_class,
             deterministic_mode):
    device_is_cuda = cfg.device.startswith('cuda')
    """do training.  may be spawned by multiprocessing if world_size > 0"""
    import torch
    torch.set_float32_matmul_precision('high')
    from torch.utils.tensorboard import SummaryWriter
    primary = True
    logging.info(f'{world_size=}')
    if world_size > 0:
        from torch.nn.parallel import DistributedDataParallel as DDP
        ddp_setup(rank, world_size)
        primary = rank == 0
    if primary:
        logging.info('start training')
        with open(f'{cfg.savefile}.json', 'w') as file:
            json.dump((cfg._asdict() | model.config), file, indent=4)
        msg = [os.path.basename(_) for _ in cfg.trainfiles] \
            if isinstance(cfg.trainfiles, tuple) else cfg.trainfiles
        logging.info(f'loading {msg}')
    dataset = miniosl.load_torch_dataset(list(cfg.trainfiles))

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
    if cfg.validation_data:
        logging.info(f'loading {cfg.validation_data} for validation')
        v_dataset = miniosl.load_torch_dataset(cfg.validation_data)
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
                vloss = do_validate(model, validate_loader,
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
                        logp = torch.nn.functional.log_softmax(
                            out.move, dim=-1
                        )
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
                    rloss,
                    move_loss.item(), value_loss.item(), aux_loss.item(),
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
                        vloss = do_validate(model, validate_loader, cfg,
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


@click.group(chain=True,
             epilog=f'The script is a part of miniosl (r{miniosl.version()}).')
@click.option('--log-level',
              type=click.Choice(['debug', 'verbose', 'warning', 'quiet'],
                                case_sensitive=False))
def main(log_level):
    """manage neural networks for miniosl

    \b
    Commands can be chainned in a squences, e.g.,
    - shogimodel build (network options) train (training options)
    - shogimodel load (filename) train (training options)
    - shogimodel load (filename) validate (validatioon options)
    - shogimodel load (filename) export (filename)
    """
    os.environ['TF_CPP_MIN_LOG_LEVEL'] = '2'
    import torch._dynamo
    if hasattr(torch._dynamo.config, 'log_level'):
        torch._dynamo.config.log_level = logging.WARNING

    level = logging.INFO
    match log_level:
        case 'debug': level = logging.DEBUG
        case 'verbose': level = logging.INFO
        case 'warning': level = logging.WARNING
        case 'quiet': level = logging.CRITICAL
    global_config['log_level'] = log_level
    miniosl.install_coloredlogs(level=level)
    global_config['logger'] = logging.getLogger(__name__)

    if deterministic_mode:
        torch.manual_seed(0)
        np.random.seed(0)
        # random.seed(0)


@main.command()
@click.argument('data', nargs=-1)
@click.option("--output", help="output path", default='')
@click.option('--device', default='cuda:0', help='device to place')
@click.option("--batch-size", type=int,
              help="batch size",
              default=1024)
@click.option("--epoch-limit", help="maximum epoch",
              type=int, default=2)
@click.option("--step-limit", help="maximum #update, 0 for inf",
              type=int, default=100000)
@click.option("--policy-commit", type=float,
              help="smoothing in cross entropy loss if <1",
              default=127/128)
@click.option("--ablate-aux-loss/--no-ablate-aux-loss", default=False)
@click.option("--report-interval", type=int, default=200)
@click.option("--validation-data", default='')
@click.option("--validation-interval", type=int, default=1000)
@click.option("--validation-step", type=int, default=100)
@click.option('--csv', default="validation.csv",
              help="file to append validation result")
def train(data, output, device, batch_size, epoch_limit, step_limit,
          policy_commit, ablate_aux_loss,
          report_interval,
          validation_data, validation_step, validation_interval, csv):
    """train a model (loaded or built in advance) with DATA (npz or sfen)"""
    if 'torch_model' in global_config:
        model = global_config['torch_model']
    else:
        logging.warn('Please load or build networks in advance')
        exit(1)
    network_cfg = model.config
    if not output:
        output = (f'model-blk{network_cfg["num_blocks"]}'
                  f'-ch{network_cfg["channels"]}'
                  f'{"-noaux" if ablate_aux_loss else ""}')

    params = sum(p.numel() for p in model.parameters() if p.requires_grad)
    logging.info(f'#trainable parameters = {params}')
    cfg = TrainingCfg(
        device=device, trainfiles=data, savefile=output,
        batch_size=batch_size, step_limit=step_limit, epoch_limit=epoch_limit,
        report_interval=report_interval,
        policy_commit=policy_commit, ablate_aux_loss=ablate_aux_loss,
        validation_data=validation_data, validation_size=validation_step,
        validation_csv=csv, validation_interval=validation_interval,
     )

    if device.startswith('cuda') and torch.cuda.device_count() > 1 \
       and not deterministic_mode:
        world_size = torch.cuda.device_count()
        port = 29500
        os.environ["MASTER_ADDR"] = "localhost"
        os.environ["MASTER_PORT"] = str(29500)
        global_config['logger'].info(f'run ddp {world_size} port {port}')
        torch.multiprocessing.spawn(
            do_train,
            args=(world_size, model, cfg, loader_class, False),
            nprocs=world_size,
            join=True)
    else:
        do_train(0, 0, model, cfg, loader_class, deterministic_mode)
    miniosl.export_model(model, device=device,
                         filename=output + '.ptd',
                         quiet=True, remove_aux_head=False)


@main.command()
@click.option("--n-block", help="#residual block",
              type=int, default=9)
@click.option("--n-channel", help="#channel", type=int, default=256)
@click.option("--broadcast-every",
              help="insert broadcast periodically",
              type=int, default=3)
@click.option('--device', default='cuda:0', help='device to place')
def build(n_block, n_channel, broadcast_every, device):
    network_cfg = {'in_channels': len(miniosl.channel_id),
                   'channels': n_channel, 'out_channels': 27,
                   'auxout_channels': miniosl.aux_unit//81,
                   'num_blocks': n_block,
                   'broadcast_every': broadcast_every}
    model = miniosl.StandardNetwork(**network_cfg).to(device)
    global_config['torch_model'] = model
    global_config['network_cfg'] = model.config


@main.command()
@click.argument('model', type=click.Path(exists=True, dir_okay=False))
@click.option('--model', type=click.Path(exists=True, dir_okay=False))
@click.option('--device', default='cuda:0', help='device to load')
@click.option('--load-strict/--no-load-strict', default=True)
@click.option('--remove-aux-head/--no-remove-aux-head', default=False)
def load(model, device, load_strict, remove_aux_head):
    """load MODEL for subsequent commands"""
    logging.info(f'loading {model}')
    network_cfg = global_config.get('network_cfg', {})
    model = miniosl.inference.load(model, device, network_cfg,
                                   compiled=False,
                                   strict=load_strict,
                                   remove_aux_head=remove_aux_head)
    global_config['model'] = model
    if isinstance(model, miniosl.inference.TorchInfer):
        global_config['torch_model'] = model.model


@main.command()
@click.argument('output', type=click.Path(exists=False, dir_okay=False))
@click.option('--device', default='cuda:0',
              help='not important except for tensorrt')
@click.option('--remove-aux-head/--no-remove-aux-head', default=False)
def export(output, device, remove_aux_head):
    """export model to OUTPUT.
    extension should be one of .onnx, .ts (tensorrt),
    ' .pts (torch script), or .ptd (raw pytorch with dict)')
    """
    quiet = global_config['log_level'] == 'quit'
    if 'torch_model' not in global_config:
        logging.warn('please load model to export in advance')
        exit(1)
    model = global_config['torch_model']
    miniosl.export_model(model, device=device,
                         filename=output,
                         quiet=quiet,
                         remove_aux_head=remove_aux_head)


@main.command()
@click.argument('data', type=click.Path(exists=True, dir_okay=False))
@click.option('--device', default='cuda:0', help='device to load')
@click.option('--csv', default="validation.csv",
              help="file to append validation result")
@click.option("--batch-size", type=int, help="batch size", default=1024)
@click.option('--step-limit', type=int, default=100,
              help="validation size")
def validate(data, device, csv, batch_size, step_limit):
    """run (only) validation on DATA"""
    v_dataset = miniosl.load_torch_dataset(data)
    v_loader = loader_class(
        v_dataset, batch_size=batch_size,
        shuffle=not deterministic_mode,
        collate_fn=lambda indices: v_dataset.collate(indices),
        num_workers=0)

    if 'model' not in global_config:
        logging.warn('please load model to export in advance')
        exit(1)
    model = global_config['model']

    logging.info(f'validation with {data},'
                 f' concurrency {miniosl.parallel_threads()}')
    cfg = ValidationCfg(device=device,
                        validation_size=step_limit, validation_csv=csv)
    ret = do_validate(model, v_loader, cfg, name='cli', main=True)
    logging.debug(f'{ret=}')


if __name__ == "__main__":
    main()

# Local Variables:
# python-indent-offset: 4
# End:
