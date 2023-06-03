from miniosl.state import State
from minioslcc import *
import miniosl.record
from miniosl.record import np_array_to_sfen_file, sfen_file_to_np_array

setattr(MiniRecord, 'replay', miniosl.record.minirecord_replay)
setattr(MiniRecord, 'to_apng', miniosl.record.minirecord_to_apng)
