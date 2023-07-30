#!/usr/bin/env python3
"""run selfplay between players.

Each player need to be defined as a subclass of :py:class:`PlayerArray`
"""
import miniosl
import miniosl.inference
import argparse
import coloredlogs
import logging
import time
import numpy as np
import os.path
import csv
import datetime

parser = argparse.ArgumentParser(
    description="conduct selfplay between policy players",
    formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument("--player-a",
                    help="first player (policy|greedy-policy|gumbel4|random)",
                    default="policy")
parser.add_argument("--player-b",
                    help="first player (policy|greedy-policy|gumbel4|random)",
                    default="policy")
parser.add_argument("--model", help="model filename", default='')
parser.add_argument("--device", help="torch device", default="cuda")
parser.add_argument("--parallel", type=int, default=1)
parser.add_argument("--n-games", help="#games to play", type=int, default=1)
grp_nn = parser.add_argument_group('options for neural networks'
                                   + ' (required for plain torch model)')
grp_nn.add_argument("--n-block", help="#residual block", type=int, default=4)
grp_nn.add_argument("--n-channel", help="#channel", type=int, default=128)
grp_nn.add_argument("--ablate-bottleneck", action='store_true')
parser.add_argument("--verbose", action='store_true')
parser.add_argument("--output", help="filename for game records",
                    default="out-sfen.txt")
parser.add_argument("--csv-output", help="filename for wins/losses",
                    default='out.csv')
parser.add_argument("--ignore-draw", action='store_true')
parser.add_argument("--noise-scale", type=float, default=1.0,
                    help="relative scale of noise, greedy if 0")
parser.add_argument("--log-level", default="INFO",
                    help="specify 'DEBUG' for verbose output")
parser.add_argument("--both-side", action='store_true')
args = parser.parse_args()

coloredlogs.install(level=args.log_level)
logger = logging.getLogger(__name__)


def save_sfen(path, record_seq):
    counts = np.zeros(4, dtype=np.int64)
    declare = 0
    with open(path, 'w') as file:
        for record in record_seq:
            print(record.to_usi(), file=file)
            counts[int(record.result)] += 1
            if record.final_move == miniosl.Move.declare_win():
                declare += 1
    logger.info(f'black {counts[int(miniosl.BlackWin)]}'
                + f'-{counts[int(miniosl.Draw)]}'
                + f'-{counts[int(miniosl.WhiteWin)]} white')
    logger.info(f'declare {declare} ({declare/len(record_seq)*100:.2f})%')
    return counts


def selfplay_array(nn_for_array: miniosl.inference.InferenceForGameArray,
                   player_a: miniosl.PlayerArray,
                   player_b: miniosl.PlayerArray,
                   output: str,
                   csv_writer   # difficult to annotate
                   ):
    logger.info(f'{player_a.name()} v.s. {player_b.name()},'
                + f' {args.n_games} games')
    mgrs = miniosl.GameArray(args.parallel, player_a, player_b, nn_for_array,
                             args.ignore_draw)
    wstart = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
    mgrs.warmup()
    wfinish = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
    welapsed = (wfinish-wstart)/(10**6)  # ms
    logger.debug(f'warmup {welapsed:.1f} ms')

    start = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
    steps = 0
    while len(mgrs.completed()) < args.n_games:
        mgrs.step()
        steps += 1
    finish = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
    elapsed = (finish-start)/(10**6)  # ms
    N = steps * args.parallel
    logger.info(f'{N} moves in {elapsed/10**3:.2f} s {elapsed/N:.2f} ms/move '
                + f'{elapsed/steps:.2f} ms/steps')
    counts = save_sfen(output, mgrs.completed())
    now = datetime.datetime.now().isoformat(timespec='seconds')
    bwin, bloss = counts[int(miniosl.BlackWin)], counts[int(miniosl.WhiteWin)]
    draw = counts[int(miniosl.Draw)]
    bwin_probability = (bwin + draw / 2) / (bwin + draw + bloss)
    eps = 1e-3
    elodiff = miniosl.p2elo(bwin_probability + eps)
    csv_writer.writerow([now,
                         player_a.name(), player_b.name(),
                         bwin, draw, bloss,
                         args.model,
                         args.device,
                         round(bwin_probability, 3),
                         round(elodiff, 1)
                         ])


def main():
    logger.info(f'load {args.model}')
    network_cfg = {'in_channels': len(miniosl.channel_id),
                   'channels': args.n_channel, 'out_channels': 27,
                   'auxout_channels': 12, 'num_blocks': args.n_block,
                   'make_bottleneck': not args.ablate_bottleneck}

    nn = miniosl.inference.load(args.model, args.device, network_cfg)
    logger.debug(f'{args.model} loaded')

    player_a = miniosl.make_player(args.player_a, noise_scale=args.noise_scale)
    player_b = miniosl.make_player(args.player_b, noise_scale=args.noise_scale)
    stub = miniosl.inference.InferenceForGameArray(nn)

    with open(args.csv_output, 'a') as csv_output:
        wr = csv.writer(csv_output, quoting=csv.QUOTE_NONNUMERIC)

        selfplay_array(stub, player_a, player_b, args.output, wr)
        if args.both_side:
            pre, ext = os.path.splitext(args.output)
            output = pre + '-r' + ext
            selfplay_array(stub, player_b, player_a, output, wr)


if __name__ == "__main__":
    main()

# Local Variables:
# python-indent-offset: 4
# End:
