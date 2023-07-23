import miniosl
import apng
import numpy as np
from collections import Counter


def minirecord_replay(self: miniosl.MiniRecord, n: int) -> miniosl.State:
    """return state after n-th move

    :param n: number of moves from initial state or the last state if negative

    >>> record = miniosl.usi_record('startpos moves 7g7f')
    >>> s = record.replay(-1)
    >>> s.piece_at(miniosl.Square(7, 6)).ptype() == miniosl.pawn
    True
    """
    if n < 0:
        n = len(self.moves) + n + 1
    s = miniosl.State(self.initial_state)
    for i, move in enumerate(self.moves):
        if i >= n:
            break
        s.make_move(move)
    return s


def subrecord_replay(self: miniosl.SubRecord, n: int) -> miniosl.State:
    """return state after n-th move

    :param n: number of moves from initial state or the last state if negative

    >>> record = miniosl.usi_record('startpos moves 7g7f')
    >>> s = record.replay(-1)
    >>> s.piece_at(miniosl.Square(7, 6)).ptype() == miniosl.pawn
    True
    """
    if len(self.moves) < n:
        n = len(self.moves)
    if n < 0:
        n = len(self.moves) + n + 1
    s = self.make_state(n)
    return s


def state_to_png(state: miniosl.State | miniosl.UI, decorate: bool) -> apng.PNG:
    """return png object of state"""
    bytes = miniosl.to_png_bytes(state.to_png(decorate=decorate))
    return apng.PNG.from_bytes(bytes)


def minirecord_to_apng(self: miniosl.MiniRecord,
                       n: int = 0, start: int = 0, delay: int = 1000,
                       decorate: bool = False) -> apng.APNG:
    """return animation of the specified range of record as apng"""
    im = apng.APNG(1)
    s = miniosl.UI(self.initial_state)
    if start <= 0:
        im.append(state_to_png(s, decorate), delay=delay)
    for i, move in enumerate(self.moves):
        s.make_move(move)
        if start < i+1:
            im.append(state_to_png(s, decorate), delay=delay)
        if n > 0 and i >= start+n:
            break
    return im


def sfen_file_to_np_array(filename: str) -> (np.ndarray, int):
    """return compressed binary moves written in sfen file"""
    data = []
    record_count = 0
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            record = miniosl.usi_record(line)
            data += record.pack_record()
            record_count += 1
    return np.array(data, dtype=np.uint64), record_count


def sfen_file_to_training_np_array(filename: str, *,
                                   with_history: bool = True,
                                   ignore_in_game: bool = True) -> np.ndarray:
    """return training data expanding positions in sfen file"""
    data = []
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            record = miniosl.usi_record(line)
            if ignore_in_game and record.result == miniosl.InGame:
                continue
            data += record.export_all320() if with_history else record.export_all()
    return np.array(data, dtype=np.uint64)


def np_array_to_sfen_file(data: np.ndarray, filename: str) -> int:
    """write sfen records compressed in data and return record count"""
    code_count = 0
    record_count = 0
    with open(filename, 'w') as f:
        while True:
            record, n = miniosl.unpack_record(data[code_count:])
            f.write(record.to_usi()+'\n')
            code_count += n
            record_count += 1
            if code_count >= len(data) or n == 0:
                break
    return record_count


def save_record_set(records: miniosl.RecordSet, filename: str,
                    name: str = '') -> None:
    """save recordset to npz by `np.savez_compressed`"""
    filename = filename if filename.endswith('.npz') else filename+'.npz'
    if name == '':
        name = 'record_set'
    data = []
    count = 0
    for r in records.records:
        data += r.pack_record()
        count += 1
    dict = {}
    dict[name] = np.array(data, dtype=np.uint64)
    dict[name+'_record_count'] = count
    np.savez_compressed(filename, **dict)


def load_record_set(path: str, name: str = '', *, limit: int | None = None):
    """load :py:class:`miniosl.RecordSet` from npz file"""
    path = path if path.endswith('.npz') else path+'.npz'
    dict = np.load(path)
    for key in dict.keys():
        data = dict[key]
        if data.ndim != 1 or not (name == '' or name == key):
            continue
        code_count = 0
        record_set = []
        while code_count < len(data):
            try:
                record, n = miniosl.unpack_record(data[code_count:])
            except Exception:
                print(f'after {len(record_set)} records, code_count {code_count}')
                raise
            record_set.append(record)
            code_count += n
            if n == 0:
                raise ValueError("malformed data")
            if limit and len(record_set) >= limit:
                break
        return miniosl.RecordSet(record_set)
    raise ValueError("data not found")


def save_opening_tree(tree: miniosl.OpeningTree,
                      filename: str) -> (np.ndarray):
    """save :py:class:`OpeningTree` to npz"""
    filename = filename if filename.endswith('.npz') else filename+'.npz'
    key_board, key_stand, node = tree.export_all()
    dict = {}
    dict['board'] = np.array(key_board, dtype=np.uint64)
    dict['stand'] = np.array(key_stand, dtype=np.uint64)
    dict['node'] = np.array(node, dtype=np.uint64)
    np.savez_compressed(filename, **dict)


def load_opening_tree(filename: str) -> miniosl.OpeningTree:
    """load :py:class:`OpeningTree` from npz file"""
    filename = filename if filename.endswith('.npz') else filename+'.npz'
    dict = np.load(filename)
    return miniosl.OpeningTree.restore_from(dict['board'], dict['stand'],
                                            dict['node'])


def retrieve_children(tree: miniosl.OpeningTree, state: miniosl.BaseState
                      ) -> list[(miniosl.OpeningTree.Node, miniosl.Move)]:
    code = state.hash_code()
    code_list = [(miniosl.hash_after_move(code, move), move)
                 for move in state.genmove()]
    children = [(tree[new_code], move)
                for new_code, move in code_list if new_code in tree]
    children.sort(key=lambda node: -node[0].count())
    return children


class SfenBlockStat:
    def __init__(self):
        self.counts = np.zeros(4, dtype=np.int32)
        self.length = []
        self.total, self.declare, self.short_draw = 0, 0, 0
        self.uniq20, self.uniq40 = Counter(), Counter()

    def add(self, record: miniosl.MiniRecord | miniosl.SubRecord) -> None:
        self.counts[int(record.result)] += 1
        if record.result == miniosl.Draw and len(record.moves) < miniosl.draw_limit:
            self.short_draw += 1
        self.total += 1
        if record.final_move == miniosl.Move.declare_win():
            self.declare += 1
        self.length.append(len(record.moves))
        code20 = record.replay(20).hash_code()
        code40 = record.replay(40).hash_code()
        self.uniq20[code20] += 1
        self.uniq40[code40] += 1

    def declare_ratio(self) -> float:
        return self.declare / self.total

    def draw_ratio(self) -> float:
        return self.counts[int(miniosl.Draw)] / self.total

    def short_draw_ratio(self) -> float:
        return self.short_draw / self.total

    def black_win_ratio(self) -> float:
        b = self.counts[int(miniosl.BlackWin)]
        w = self.counts[int(miniosl.WhiteWin)]
        return  b / (b + w)

    def uniq20_ratio(self) -> float:
        return len(self.uniq20) / self.total

    def uniq40_ratio(self) -> float:
        return len(self.uniq40) / self.total
