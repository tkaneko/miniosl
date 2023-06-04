import miniosl
import apng
import base64
import numpy as np


def minirecord_replay(self: miniosl.MiniRecord, n: int) -> miniosl.State:
    """return state after n-th move"""
    if n < 0:
        n = len(self.moves) - n
    s = miniosl.state.State(self.initial_state)
    for i, move in enumerate(self.moves):
        if i >= n:
            break
        s.make_move(move)
    return s


def state_to_png(state: miniosl.State, decorate: bool) -> apng.PNG:
    """return png object of state"""
    b64png = state.to_png(decorate=decorate).as_data_uri()
    bytes = b64png[len('data:image/png;base64,'):]
    return apng.PNG.from_bytes(base64.b64decode(bytes.encode('utf-8')))


def minirecord_to_apng(self: miniosl.MiniRecord,
                       n: int = 0, start: int = 0, delay: int = 1000,
                       decorate: bool = False) -> apng.APNG:
    """return animation of the specified range of record as apng"""
    im = apng.APNG(1)
    s = miniosl.state.State(self.initial_state)
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


def sfen_file_to_training_np_array(filename: str) -> np.ndarray:
    """return training data expanding positions in sfen file"""
    data = []
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            record = miniosl.usi_record(line)
            data += record.export_all()
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
