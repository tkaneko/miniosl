from miniosl.ui import UI
from minioslcc import *
import miniosl.record
from miniosl.record import np_array_to_sfen_file, sfen_file_to_np_array, \
    sfen_file_to_training_np_array

setattr(miniosl.MiniRecord, 'replay', miniosl.record.minirecord_replay)
setattr(miniosl.MiniRecord, 'to_apng', miniosl.record.minirecord_to_apng)

setattr(miniosl.BaseState, 'to_svg', miniosl.drawing.state_to_svg)
setattr(miniosl.BaseState, 'to_png', miniosl.drawing.state_to_png)

setattr(miniosl.State, 'to_svg', miniosl.drawing.state_to_svg)
setattr(miniosl.State, 'to_png', miniosl.drawing.state_to_png)
