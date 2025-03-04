"""convert game records of shogi"""
import miniosl
import click
import tqdm
import logging
import glob
import os.path

miniosl.install_coloredlogs()
logger = logging.getLogger(__name__)


def load_records(path: str):
    if path.endswith('.npz'):
        return miniosl.RecordSet.from_npz(path).records
    if 'sfen' in path:
        return miniosl.RecordSet.from_usi_file(path).records
    if path.endswith('.csa'):
        return miniosl.MiniRecordVector(
            [miniosl.csa_file(path)]
        )
    if path.endswith('.kif'):
        logger.warn('handling of .kif files is an experimental feature')
        return miniosl.MiniRecordVector(
            [miniosl.kif_file(path)]
        )
    if path.endswith('.ki2'):
        logger.warn('handling of .ki2 files is an experimental feature')
        return miniosl.MiniRecordVector(
            [miniosl.load_ki2(path)]
        )
    if os.path.isdir(path):
        paths = glob.iglob(path + '/*.csa')
        records = []
        for p in paths:
            record_set = load_records(p)
            records += record_set.records
        return miniosl.MiniRecordVector(records)
    raise ValueError('not implemented')


@click.group()
def main():
    pass


def safe_load(path, make_editable: bool = False):
    try:
        book = miniosl.load_opening_tree(path)
        return book.mutable_clone() if make_editable else book
    except Exception as e:
        logger.error(f'load {path} failed by {e}')
    return None


@main.command()
@click.argument('files', nargs=-1)
@click.option('--load', default='', help='path for initial data')
@click.option('--output', default='book.bin', help="path to output",
              show_default=True)
@click.option('--decay-interval', type=int, help="#files before each decay",
              default=50, show_default=True)
@click.option('--dfs-threshold', type=int, help="threshold for dfs",
              default=128, show_default=True)
def build(load, output, files, decay_interval, dfs_threshold):
    """build a book consist of FILES"""
    book = miniosl.OpeningTreeEditable()
    if load:
        if loaded := safe_load(load, make_editable=True):
            book = loaded
    records = []
    with tqdm.tqdm(total=len(files), leave=False) as pbar:
        for i, path in enumerate(files):
            pbar.set_description(os.path.basename(path))
            records = load_records(path)
            book.add_all(records)
            if decay_interval and (i+1) % decay_interval == 0:
                book.decay_all()
            pbar.update(1)

    visited = book.dfs(dfs_threshold)
    logger.info(f'visited {visited} nodes by dfs')
    if output:
        book.save_binary(output)


@main.command()
@click.argument('load', type=click.Path(exists=True))
@click.option('--threshold', type=int, default=2, show_default=True)
@click.option('--output', default='book.bin', show_default=True)
def prune(load, threshold, output):
    """prune a book by visiting threshold"""
    book = miniosl.OpeningTree.load_binary(load)
    book.prune(threshold, output)


@main.command()
@click.argument('load', type=click.Path(exists=True))
@click.option('--path', help="csa moves concatinated w/o delimiter")
@click.option('--width', help="#moves to show at the root",
              type=int, default=8, show_default=True)
@click.option('--second-width',
              help="#moves to show at the first child of the root",
              type=int, default=4, show_default=True)
@click.option('--eval',
              type=click.Path(exists=True),
              help='model to load')
@click.option('--aozora/--no--aozora',
              default=False,
              help='root is for aozora shogi')
def inspect(load, path, width, second_width, eval, aozora):
    """inspect a specified book"""
    import rich
    import rich.tree
    book = safe_load(load)
    if not book:
        return
    book_size = book.size()
    cnt_width = len(str(book_size))
    if aozora:
        ui = miniosl.UI(miniosl.aozora(), prefer_text=True)
    else:
        ui = miniosl.UI(prefer_text=True)
    if eval:
        import torch
        device = "cuda" if torch.cuda.is_available() else "cpu"
        ui.load_eval(eval, device=device)

    state = ui._state
    root_sgn = state.turn.sign
    policy, root_value = {}, 0
    if eval:
        root_value, policy = ui.eval()
        ptotal = sum([_[0] for _ in policy])
        policy = {_[1]: _[0] / ptotal for _ in policy}
        # print(policy)

    children = book.retrieve_children(state)
    if not children:
        logger.error('root not found')
        return 1
    bwin = sum([node[0] for node, _ in children])
    wwin = sum([node[1] for node, _ in children])
    draw = sum([node[2] for node, _ in children])
    brate = (bwin + draw/2) / sum([bwin, wwin, draw]) if children else 0
    total = sum([node.count() for node, _ in children])

    root_label = f'{os.path.basename(load)} {book_size}'
    root_label += f'  {total} bwin {brate:5.3f}'
    if eval:
        root_label += f' bv {root_sgn * root_value[0]:6.1f}'
    root_info = book[state.hash_code()]
    root_label += f' {root_info[0]}-{root_info[2]}-{root_info[1]}' \
        + f' ({root_info[3]})'
    tree = rich.tree.Tree(root_label)
    root = tree

    if path:
        path = path.replace('+', ' +').replace('-', ' -').split()
        for edge in path:
            ui.make_move(edge)
            node = [
                node for node, move in children if move.to_csa() == edge
            ][0]
            children = book.retrieve_children(state)
            elabel = (
                f'{node.depth} {edge} '
                f'freq {node.count():{cnt_width}} {node.count()/total:.3f}'
                f' bwin {node.black_advantage():5.3f}'
            )
            if node.black_value_backup:
                elabel += f' {node.black_value_backup:5.3f}'
            if eval:
                root_value, policy = ui.eval()
                ptotal = sum([_[0] for _ in policy])
                policy = {_[1]: _[0] / ptotal for _ in policy}
                elabel += f' bv {root_sgn * root_value[0]:6.1f}'
            root = root.add(elabel)

    for i, (node, move) in enumerate(children[:width]):
        ui.make_move(move)
        state = ui._state
        pii = f'pi {policy[move]:.3f}' if move in policy else ''
        clabel = (
            f'{node.depth} [orange3]{move.to_csa()}[/orange3] '
            f'freq {node.count():{cnt_width}} {node.count()/total:.3f}'
            f' bwin [orange3]{node.black_advantage():5.3f}[/orange3] {pii}'
        )
        if node.black_value_backup:
            clabel += f' {node.black_value_backup:5.3f}'
        if eval:
            cvalue, cpolicy = ui.eval()
            ctotal = sum([_[0] for _ in cpolicy])
            cpolicy = {_[1]: _[0] / ctotal for _ in cpolicy}
            clabel += f' bv {-root_sgn * cvalue[0]:6.1f}'
        node = root.add(clabel)
        if i < second_width:
            second_children = book.retrieve_children(state)[:second_width-i]
            for cnode, cmove in second_children:
                ui.make_move(cmove)
                state = ui._state
                cclabel = (
                    f'{cnode.depth} [sky_blue3]{cmove.to_csa()}[/sky_blue3]'
                    f' [gray50]freq[/gray50] {cnode.count():{cnt_width}}'
                    f' {cnode.count()/total:.3f}'
                    ' [gray50]bwin[/gray50] [sky_blue3]'
                    f'{cnode.black_advantage():5.3f}[/sky_blue3]'
                )
                if cnode.black_value_backup:
                    cclabel += f' {cnode.black_value_backup:5.3f}'
                if eval:
                    if cmove in cpolicy:
                        cclabel += f' [gray50]pi[/gray50] {cpolicy[cmove]:.3f}'
                    ccvalue, _ = ui.eval()
                    cclabel += (
                        f' [gray50]bv[/gray50] {root_sgn * ccvalue[0]:6.1f}'
                    )
                node.add(cclabel)
                ui.unmake_move()
                state = ui._state
        ui.unmake_move()
        state = ui._state

    rich.print(tree)


@main.command()
@click.argument('src', type=click.Path(exists=True))
@click.argument('dst')
def convert(src, dst):
    """prune a book by visiting threshold"""
    book = safe_load(src)
    if dst.endswith('.npz'):
        book.save_npz(dst)
    else:
        book.save_binary(dst)


@main.command()
@click.argument('book', type=click.Path(exists=True))
@click.option('--threshold',
              help="threshold to visit",
              type=int, default=128, show_default=True)
@click.option('--output')
def dfs(book, threshold, output):
    """prune a book by visiting threshold"""
    book = safe_load(book, make_editable=True)
    visited = book.dfs(threshold)
    logger.info(f'visited {visited} nodes by dfs')
    if output:
        if output.endswith('.npz'):
            book.save_npz(output)
        else:
            book.save_binary(output)


if __name__ == "__main__":
    main()
