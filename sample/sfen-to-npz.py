import miniosl
import argparse
import numpy as np
import os.path


parser = argparse.ArgumentParser("compress sfen lines into npz")
parser.add_argument("--npz", help="npz filename", default="sfen")
group = parser.add_mutually_exclusive_group()
group.add_argument('--sfen', nargs="+", help="filenames of sfen lines")
group.add_argument("--decompress", help="unpack sfen from npz",
                   action='store_true')
args = parser.parse_args()


def compress_sfen(args):
    dict = {}
    for name in args.sfen:
        print("packing", name, "as", os.path.basename(name))
        dict[os.path.basename(name)] = miniosl.sfen_file_to_np_array(name)
    np.savez_compressed(args.npz, **dict)


def de_compress_sfen(args):
    filename = args.npz if args.npz.endswith('.npz') else args.npz+'.npz'
    dict = np.load(filename)
    for name in dict.keys():
        print("unpacking", name)
        miniosl.np_array_to_sfen_file(dict[name], name)


if args.decompress:
    de_compress_sfen(args)
else:
    compress_sfen(args)
