from __future__ import annotations
import minioslcc
import miniosl.state
import apng
import base64
import numpy as np


def minirecord_replay(self: minioslcc.MiniRecord, n: int) -> State:
    if n < 0:
        n = len(self.moves) - n
    s = miniosl.state.State(self.initial_state)
    for i, move in enumerate(self.moves):
        if i >= n:
            break
        s.make_move(move)
    return s


def state_to_png(state: State, decorate: bool) -> apng.PNG:
    b64png = state.to_png(decorate=decorate).as_data_uri()
    bytes = b64png[len('data:image/png;base64,'):]
    return apng.PNG.from_bytes(base64.b64decode(bytes.encode('utf-8')))


def minirecord_to_apng(self: minioslcc.MiniRecord,
                       n: int = 0, start: int = 0, delay: int = 1000,
                       decorate: bool = False) -> apng.APNG:
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


def sfen_file_to_np_array(filename: str) -> np.ndarray:
    data = []
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            record = miniosl.usi_record(line)
            data += record.pack_record()
    return np.array(data, dtype=np.uint64)


def np_array_to_sfen_file(data: np.ndarray, filename: str) -> None:
    count = 0
    with open(filename, 'w') as f:
        while True:
            record, n = miniosl.unpack_record(data[count:])
            f.write(record.to_usi()+'\n')
            count += n
            if count >= len(data) or n == 0:
                break
