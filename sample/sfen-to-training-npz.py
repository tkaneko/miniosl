import miniosl
import argparse
import numpy as np
import os.path


parser = argparse.ArgumentParser("read sfen files and generate training data as npz")
parser.add_argument("--npz", help="npz filename", default="positions")
group = parser.add_mutually_exclusive_group()
group.add_argument('--sfen', nargs="+", help="filenames of sfen lines")
# group.add_argument("--decompress", help="unpack sfen positions from npz",
#                    action='store_true')
args = parser.parse_args()


def generate(args):
    dict = {}
    for name in args.sfen:
        print("reading", name, end="\t", flush=True)
        basename = os.path.basename(name)
        dataset = miniosl.sfen_file_to_training_np_array(name)
        np.random.shuffle(dataset)
        dict[basename] = dataset
        print(len(dataset), "positions")
    np.savez_compressed(args.npz, **dict)


generate(args)
