import miniosl
import argparse
import numpy as np
import os.path
import logging
FORMAT = '%(asctime)s %(levelname)s %(message)s'
logging.basicConfig(format=FORMAT, level=logging.INFO)


parser = argparse.ArgumentParser("sfen-to-npz.py")
parser.add_argument("--npz", help="npz filename", default="sfen")
group = parser.add_mutually_exclusive_group(required=True)
group.add_argument('--sfen', nargs="+", help="filenames of sfen lines")
group.add_argument("--decompress", help="unpack sfen from npz",
                   action='store_true')
args = parser.parse_args()


def compress_sfen(args):
    dict = {}
    for name in args.sfen:
        basename = os.path.basename(name)
        logging.info(f"packing {name} as {basename}")
        data, count = miniosl.sfen_file_to_np_array(name)
        dict[basename] = data
        dict[basename+'_record_count'] = count
    np.savez_compressed(args.npz, **dict)


def test_count_consistency(dict, name, written):
    count_key = name+'_record_count'
    if count_key in dict:
        expected = dict[count_key]
        if expected == written:
            logging.info(f'successfully wrote {written} records')
        else:
            logging.error(f'count mismatch {expected} vs {written}')
    else:
        logging.info(f'wrote {written} records')


def de_compress_sfen(args):
    filename = args.npz if args.npz.endswith('.npz') else args.npz+'.npz'
    dict = np.load(filename)
    for name in dict.keys():
        data = dict[name]
        if data.ndim == 1:
            logging.info(f"unpacking {name}")
            written = miniosl.np_array_to_sfen_file(data, name)
            test_count_consistency(dict, name, written)
        elif data.ndim > 1:
            logging.warning(f'ignore {name} of shape {data.shape}')


if args.decompress:
    de_compress_sfen(args)
else:
    compress_sfen(args)
