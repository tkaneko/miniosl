"""convert game records of shogi"""
import miniosl
import argparse
import coloredlogs
import logging
import copy
import glob
import os.path

coloredlogs.install(level='INFO')
logger = logging.getLogger(__name__)

parser = argparse.ArgumentParser(description="convert game records")
parser.add_argument("--input", help="file or folder name")
parser.add_argument("--output",
                    help="filename ends with '.csa', '.sfen', or '.sfen.npz'")

args = parser.parse_args()


def load_records(path: str):
    if path.endswith('.npz'):
        return miniosl.RecordSet.from_npz(path)
    if 'sfen' in path:
        return miniosl.RecordSet.from_usi_file(path)
    if path.endswith('.csa'):
        return miniosl.RecordSet([miniosl.csa_file(path)])
    if os.path.isdir(path):
        paths = glob.iglob(path + '/*.csa')
        records = []
        for p in paths:
            record_set = load_records(p)
            records += record_set.records
        return miniosl.RecordSet(records)
    raise ValueError('not implemented')


def save_csa(record, path):
    with open(path, 'w') as out:
        out.write(record.initial_state.to_csa())
        for move in record.moves:
            out.write(move.to_csa() + '\n')
        if record.result != miniosl.InGame:
            out.write(record.final_move.to_csa() + '\n')


def save_ki2(record_set, path):
    with open(path, 'w') as out:
        for record in record_set.records:
            state = copy.copy(record.initial_state)
            prev = miniosl.Square()
            for move in record.moves:
                out.write(miniosl.to_ja(move, state, prev))
                state.make_move(move)
                prev = move.dst()
            if record.result != miniosl.InGame:
                out.write(miniosl.to_ja(record.final_move, state))
            out.write('\n')


def save_records(record_set: miniosl.RecordSet, path: str):
    if path.endswith('sfen.npz'):
        return record_set.save_npz(path)

    if path.endswith('.csa'):
        if len(record_set) > 1:
            raise ValueError('csa require single record')
        return save_csa(record_set.records[0], path)

    if path.endswith('.ki2'):
        return save_ki2(record_set, path)

    if not path.endswith('sfen'):
        logging.warning(f'assume sfen for unknown format {path}')
    with open(path, 'w') as out:
        for record in record_set.records:
            print(record.to_usi(), file=out)


def main():
    record_set = load_records(args.input)

    stat = miniosl.SfenBlockStat(record_set.records)
    logging.info(f'load {len(record_set)} records'
                 f' uniq20 {stat.uniq20_ratio()*100:.1f}%,'
                 f' uniq40 {stat.uniq40_ratio()*100:.1f}%'
                 f' draw {stat.short_draw/stat.total*100:.1f}%')
    if stat.zero_moves:
        logging.warning(f'there are {stat.zero_moves} empty records')

    save_records(record_set, args.output)


if __name__ == "__main__":
    main()
