"""convert sfen to npz for supervised training."""
from miniosl import sfen_file_to_training_np_array
import argparse
import logging
import numpy as np
import os.path
FORMAT = '%(asctime)s %(levelname)s %(message)s'
logging.basicConfig(format=FORMAT, level=logging.INFO, force=True)


parser = argparse.ArgumentParser("sfen-to-training-npz.py")
parser.add_argument("--npz", help="npz filename", default="positions")
group = parser.add_mutually_exclusive_group()
group.add_argument('--sfen', nargs="+", help="filenames of sfen lines")
parser.add_argument("--without-history", help="exclude history in each record",
                    action='store_true')
args = parser.parse_args()


def generate(args):
    dict = {}
    for name in args.sfen:
        logging.info(f"reading {name}")
        basename = os.path.basename(name)
        with_history = not args.without_history
        dataset = \
            sfen_file_to_training_np_array(name, with_history=with_history)
        dict[basename] = dataset
        logging.info(f"{len(dataset)} positions")
    np.savez_compressed(args.npz, **dict)


if __name__ == '__main__':
    generate(args)
