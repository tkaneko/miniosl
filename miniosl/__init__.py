from miniosl.state import State
from minioslcc import *
import miniosl.record
from miniosl.record import np_array_to_sfen_file, sfen_file_to_np_array, \
  sfen_file_to_training_np_array

setattr(miniosl.MiniRecord, 'replay', miniosl.record.minirecord_replay)
setattr(miniosl.MiniRecord, 'to_apng', miniosl.record.minirecord_to_apng)

setattr(miniosl.BaseState, 'to_svg',
        lambda state, id=0, *, plane=None:
        miniosl.drawing.state_to_svg(state, id, plane=plane))
setattr(miniosl.BaseState, 'to_png',
        lambda state, id=0, *, plane=None:
        miniosl.drawing.state_to_svg(state, id, plane=plane).rasterize())
