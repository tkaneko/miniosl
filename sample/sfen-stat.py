"""statistics for sfen file"""
import miniosl
import argparse
import numpy as np
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser(
    description="statistics",
    formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument("files", nargs='+', help="sfen file to process")
parser.add_argument("--plot", help="sfen file to process", default='')
args = parser.parse_args()


if __name__ == "__main__":
    all = []
    for filename in args.files:
        print(filename)
        table = miniosl.SfenBlockStat()
        with open(filename, 'r') as file:
            for line in file:
                record = miniosl.usi_record(line)
                table.add(record)

        all.append(table.length)
        print(f'  black {table.counts[int(miniosl.BlackWin)]}'
              + f'-{table.counts[int(miniosl.Draw)]}'
              + f'-{table.counts[int(miniosl.WhiteWin)]} white')
        if table.counts[int(miniosl.InGame)]:
            print(f'  in game {table.counts[int(miniosl.InGame)]}')
        print(f'  declare {table.declare} ({table.declare_ratio()*100:.3f})%')
        print(f'  moves mean {np.mean(table.length):.2f} (std {np.std(table.length):.3f})'
              + f' [{np.min(table.length)}, {np.median(table.length):.0f}, {np.max(table.length)}]')
        print(f'  uniqueness {table.uniq20_ratio():.3f} at 20th, {table.uniq40_ratio():.3f} at 40th')
        if not args.plot:
            counts, bins = np.histogram(table.length, list(range(40, 321, 40)))
            print('  ', counts, '\n    ', bins)
    if args.plot:
        labels = [_.replace('.sfen', '') for _ in args.files]
        plt.hist(all, 8, histtype='bar', label=labels)  # density=True,
        plt.legend(prop={'size': 10})
        # plt.show()
        plt.savefig(args.plot)
        print(f'wrote {args.plot}')
