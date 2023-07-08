import miniosl
import argparse
import logging
import numpy as np
import os.path
FORMAT = '%(asctime)s %(levelname)s %(message)s'
logging.basicConfig(format=FORMAT, level=logging.DEBUG, force=True)


parser = argparse.ArgumentParser("sfen-to-training-npz.py")
parser.add_argument("--npz", help="npz filename", default="positions")
group = parser.add_mutually_exclusive_group()
group.add_argument('--sfen', nargs="+", help="filenames of sfen lines")
parser.add_argument("--with-history", help="add history in each record",
                    action='store_true')
parser.add_argument("--shuffle", help="shuffle record",
                    action='store_true')
args = parser.parse_args()


def generate(args):
    dict = {}
    for name in args.sfen:
        logging.info(f"reading {name}")
        basename = os.path.basename(name)
        dataset = miniosl.sfen_file_to_training_np_array(name, with_history=args.with_history)
        if args.shuffle:
            np.random.shuffle(dataset)
        dict[basename] = dataset
        logging.info(f"{len(dataset)} positions")
    np.savez_compressed(args.npz, **dict)


generate(args)
