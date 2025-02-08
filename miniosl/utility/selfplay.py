#!/usr/bin/env python3
"""run selfplay between players.

Each player need to be defined as a subclass of :py:class:`PlayerArray`
"""
import miniosl
import miniosl.inference
import argparse
import logging
import time
import numpy as np
import os.path
import csv
import datetime
import tqdm
import threading


def handle_games(record_seq, outfile=None):
    counts = np.zeros(4, dtype=np.int64)
    declare = 0
    for record in record_seq:
        if outfile:
            print(record.to_usi(), file=outfile)
        counts[int(record.result)] += 1
        if record.final_move == miniosl.Move.declare_win():
            declare += 1
    return counts, declare


def save_sfen(record_seq, path=''):
    if path:
        with open(path, 'w') as file:
            counts, declare = handle_games(record_seq, file)
    else:
        counts, declare = handle_games(record_seq)
    logging.info(f'black {counts[int(miniosl.BlackWin)]}'
                 f'-{counts[int(miniosl.Draw)]}'
                 f'-{counts[int(miniosl.WhiteWin)]} white'
                 f' declare {declare} ({declare/len(record_seq)*100:.2f})%')
    return counts


eps = 1e-4


def shorten(name):
    if len(name) >= 20:
        name = name[:14] + '...' + name[-3:]
    return name


def selfplay_array(
        cfg,
        nn_for_array: list[miniosl.inference.InferenceForGameArray],
        nn_for_array_b: list[miniosl.inference.InferenceForGameArray],
        player_a: list[miniosl.PlayerArray],
        player_b: list[miniosl.PlayerArray],
        output: str,
        csv_writer   # difficult to annotate
):
    logging.info(f'{shorten(player_a[0].name())}'
                 + f' v.s. {shorten(player_b[0].name())},'
                 + f' {cfg.n_games} games')
    game_config = miniosl.GameConfig()
    game_config.ignore_draw = cfg.ignore_draw
    game_config.variant = miniosl.Hirate
    if cfg.shogi816k:
        game_config.variant = miniosl.Shogi816K
    elif cfg.aozora:
        game_config.variant = miniosl.Aozora
    mgrs = [miniosl.GameArray(cfg.parallel, pa, pb, na, nb,
                              game_config)
            for pa, pb, na, nb in zip(
                    player_a, player_b,
                    nn_for_array, nn_for_array_b
            )]
    # mgrs = [miniosl.GameArray(cfg.parallel, player_a[0], player_b[0],
    #                           nn_for_array[0], nn_for_array_b[0],
    #                           game_config)]
    wstart = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
    for m in mgrs:
        m.warmup()
    wfinish = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
    welapsed = (wfinish-wstart)/(10**6)  # ms
    logging.debug(f'warmup {welapsed:.1f} ms')

    start = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
    steps = np.zeros(len(mgrs), dtype=int)
    prevs = np.zeros(len(mgrs), dtype=int)

    def total_completed(mgrs):
        return sum([len(_.completed()) for _ in mgrs])

    def task(id, pbar):
        while total_completed(mgrs) < cfg.n_games:
            mgrs[id].step()
            steps[id] += 1
            remains = cfg.n_games - prevs.sum()
            done = len(mgrs[id].completed())
            inc = done - prevs[id]
            pbar.update(min(remains, inc))
            prevs[id] = done

    bar_disable = not logging.getLogger(__name__).isEnabledFor(logging.INFO)
    bar_format = \
        "{l_bar}{bar}| {n_fmt}/{total_fmt}[{elapsed}<{remaining}{postfix}]"
    with tqdm.tqdm(total=cfg.n_games,
                   bar_format=bar_format,
                   disable=bar_disable, leave=False) as pbar:
        threads = [threading.Thread(target=task, args=[id, pbar])
                   for id in range(len(mgrs))]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
    finish = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
    elapsed = (finish-start)/(10**6)  # ms
    N = steps.sum() * cfg.parallel
    logging.info(f'{N} moves in {elapsed/10**3:.2f}s, {elapsed/N:.2f}ms/move, '
                 + f'{elapsed/steps.sum():.2f}ms/steps')
    counts = save_sfen(sum([list(_.completed()) for _ in mgrs], []), output)
    now = datetime.datetime.now().isoformat(timespec='seconds')
    bwin, bloss = counts[int(miniosl.BlackWin)], counts[int(miniosl.WhiteWin)]
    draw = counts[int(miniosl.Draw)]
    bwin_probability = (bwin + draw / 2) / (bwin + draw + bloss)
    elodiff = miniosl.p2elo(min(bwin_probability + eps, 1-eps))
    if csv_writer and not cfg.both_side:
        csv_writer.writerow([now,
                             player_a[0].name(), player_b[0].name(),
                             bwin, draw, bloss,
                             cfg.model,
                             cfg.device,
                             round(bwin_probability, 3),
                             round(elodiff, 1)
                             ])
    games = [len(_.completed()) for _ in mgrs]
    if max(games) - min(games) > cfg.n_games // 2:
        logging.warning(f'games {games}')
    return bwin, draw, bloss


def main():
    parser = argparse.ArgumentParser(
        description="conduct selfplay between policy players",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument(
        "--player-a",
        help="first player (policy|greedy-policy|gumbel4|random)",
        default="policy")
    parser.add_argument(
        "--player-b",
        help="first player (policy|greedy-policy|gumbel4|random)",
        default="policy")
    parser.add_argument(
        "--model", help="model filename",
        default=str(miniosl.pretrained_eval_path())
        if miniosl.has_pretrained_eval() else '')
    parser.add_argument(
        "--model-b",
        help="model filename for player b (--model governs if uncpecified)",
        default='')
    parser.add_argument("--book", help="book filename")
    parser.add_argument("--device", help="torch device", default="cuda")
    parser.add_argument("--device-b", help="torch device for player b",
                        default='')
    parser.add_argument("--parallel", type=int, default=1)
    parser.add_argument("--n-games", help="#games to play",
                        type=int, default=1)
    grp_nn = parser.add_argument_group('options for neural networks'
                                       + ' (required for plain torch model)')
    grp_nn.add_argument("--n-block", help="#residual block",
                        type=int, default=4)
    grp_nn.add_argument("--n-channel", help="#channel", type=int, default=128)
    grp_nn.add_argument("--ablate-bottleneck", action='store_true')
    parser.add_argument("--verbose", action='store_true')
    parser.add_argument("--output", help="filename for game records",
                        default="selfplay-sfen.txt")
    parser.add_argument("--csv-output", help="filename for wins/losses",
                        default='selfplay.csv')
    parser.add_argument("--ignore-draw", action='store_true')
    parser.add_argument("--shogi816k", action='store_true',
                        help='use randomized initial state')
    parser.add_argument("--aozora", action='store_true',
                        help='game variant removing all pawns')
    parser.add_argument("--noise-scale", type=float, default=1.0,
                        help="relative scale of noise, greedy if 0")
    parser.add_argument("--log-level", default="INFO",
                        help="specify 'DEBUG' for verbose output")
    parser.add_argument("--both-side", action='store_true')
    parser.add_argument("--n-sites", type=int, default=1, help="")
    parser.add_argument("--softalpha", type=float, default=0.00783,
                        help="alpha for soft player")
    parser.add_argument("--depth-weight", type=float, default=0.5,
                        help="depth weight effective for some players, "
                        "e.g., gumbel8-2")
    args = parser.parse_args()

    miniosl.install_coloredlogs(level=args.log_level)
    logger = logging.getLogger(__name__)
    logger.info(f'load {args.model}')
    network_cfg = {'in_channels': len(miniosl.channel_id),
                   'channels': args.n_channel, 'out_channels': 27,
                   'auxout_channels': 12, 'num_blocks': args.n_block,
                   'make_bottleneck': not args.ablate_bottleneck}

    nn = [miniosl.inference.load(args.model, args.device, network_cfg)
          for _ in range(args.n_sites)]
    logger.debug(f'{args.model} loaded')
    if args.model_b:
        logger.info(f'load {args.model_b}')
        nn_b = [miniosl.inference.load(
            args.model_b,
            args.device_b or args.device,
            network_cfg
        ) for _ in range(args.n_sites)]

    player_a = [miniosl.make_player(
        args.player_a,
        noise_scale=args.noise_scale, softalpha=args.softalpha,
        depth_weight=args.depth_weight, book_path=args.book,
    ) for _ in range(args.n_sites)]
    player_b = [miniosl.make_player(
        args.player_b,
        noise_scale=args.noise_scale, softalpha=args.softalpha,
        depth_weight=args.depth_weight, book_path=args.book,
    ) for _ in range(args.n_sites)]
    stub = [miniosl.inference.InferenceForGameArray(_)
            for _ in nn]
    stub_b = [miniosl.inference.InferenceForGameArray(_)
              for _ in nn_b] if args.model_b else stub

    with open(args.csv_output, 'a') as csv_output:
        wr = csv.writer(csv_output, quoting=csv.QUOTE_NONNUMERIC)

        bwin, bdraw, _ = selfplay_array(args,
                                        stub, stub_b,
                                        player_a, player_b,
                                        args.output, wr)
        if args.both_side:
            pre, ext = os.path.splitext(args.output)
            output = pre + '-r' + ext
            _, wdraw, wwin = selfplay_array(
                args, stub_b, stub, player_b, player_a,
                output, wr
            )
            p = (bwin + wwin + bdraw/2 + wdraw/2) / (args.n_games * 2)
            elodiff = miniosl.p2elo(min(p + eps, 1-eps))
            logger.info(f'{p:.3f},{elodiff:.2f}')
            wr.writerow([round(p, 3), round(elodiff, 1)])


if __name__ == "__main__":
    main()

# Local Variables:
# python-indent-offset: 4
# End:
