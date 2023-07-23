"""run selfplay between players.

Each player should be a subclass of :py:class:`PlayerArray`
"""
import miniosl
import miniosl.inference
import argparse
import coloredlogs
import logging
import time
import numpy as np

coloredlogs.install(level='DEBUG')
logger = logging.getLogger(__name__)

parser = argparse.ArgumentParser(
    description="conduct selfplay between policy players",
    formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument("--player-a",
                    help="first player (policy|greedy-policy|gumbel4|random)",
                    default="policy")
parser.add_argument("--player-b",
                    help="first player (policy|greedy-policy|gumbel4|random)",
                    default="policy")
parser.add_argument("modelfile", help="model filename", default='')
parser.add_argument("--device", help="torch device", default="cuda")
parser.add_argument("--parallel", type=int, default=1)
parser.add_argument("--n-games", help="#games to play", type=int, default=1)
grp_nn = parser.add_argument_group('options for neural networks'
                                   + ' (required for plain torch model)')
grp_nn.add_argument("--n-block", help="#residual block", type=int, default=4)
grp_nn.add_argument("--n-channel", help="#channel", type=int, default=128)
grp_nn.add_argument("--ablate-bottleneck", action='store_true')
parser.add_argument("--verbose", action='store_true')
parser.add_argument("--output", help="filename", default="out-sfen.txt")
args = parser.parse_args()


def save_sfen(path, record_seq):
    counts = np.zeros(4)
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
    logger.info(f'declare {declare} ({declare/len(record_seq)*100})%')


def selfplay_array(nn, cfg):
    stub = miniosl.inference.InferenceForGameArray(nn)
    player_a = miniosl.make_player(args.player_a)
    player_b = miniosl.make_player(args.player_b)
    logger.info(f'{player_a.name()} v.s. {player_b.name()},'
                + f'{args.n_games} games')
    mgrs = miniosl.GameArray(args.parallel, player_a, player_b, stub)
    wstart = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
    mgrs.warmup()
    wfinish = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
    welapsed = (wfinish-wstart)/(10**6)  # ms
    logger.info(f'warmup {welapsed} ms')

    start = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
    steps = 0
    while len(mgrs.completed()) < args.n_games:
        mgrs.step()
        steps += 1
    finish = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
    elapsed = (finish-start)/(10**6)  # ms
    N = steps * args.parallel
    logger.info(f'{N} moves in {elapsed/10**3:.2f}s {elapsed/N:.2f}ms/move '
                + f'{elapsed/steps:.2f}ms/steps')
    save_sfen(args.output, mgrs.completed())


if __name__ == "__main__":
    logger.info(f'load {args.modelfile}')
    network_cfg = {'in_channels': len(miniosl.channel_id),
                   'channels': args.n_channel, 'out_channels': 27,
                   'auxout_channels': 12, 'num_blocks': args.n_block,
                   'make_bottleneck': not args.ablate_bottleneck}

    nn = miniosl.inference.load(args.modelfile, args.device, network_cfg)
    logger.info(f'{args.modelfile} loaded')

    selfplay_array(nn, args)

# Local Variables:
# python-indent-offset: 4
# End:
