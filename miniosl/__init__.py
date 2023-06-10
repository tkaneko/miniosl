from miniosl.state import State
from minioslcc import *
import miniosl.record
from miniosl.record import np_array_to_sfen_file, sfen_file_to_np_array, \
    sfen_file_to_training_np_array

setattr(miniosl.MiniRecord, 'replay', miniosl.record.minirecord_replay)
setattr(miniosl.MiniRecord, 'to_apng', miniosl.record.minirecord_to_apng)


def base_state_to_svg(state: miniosl.BaseState, id=0, *, plane=None):
    return miniosl.drawing.state_to_svg(state, id, plane=plane)


def base_state_to_png(state: miniosl.BaseState, id=0,
                      *, plane=None, filename=''):
    png = miniosl.drawing.state_to_svg(state, id, plane=plane).rasterize()
    return png if not filename else miniosl.drawing.save_png(png, filename)


setattr(miniosl.BaseState, 'to_svg', base_state_to_svg)
setattr(miniosl.BaseState, 'to_png', base_state_to_png)
