from miniosl.ui import UI
from minioslcc import *
from .record import np_array_to_sfen_file, sfen_file_to_np_array, \
    sfen_file_to_training_np_array, load_record_set, load_opening_tree

setattr(MiniRecord, 'replay', record.minirecord_replay)
setattr(MiniRecord, 'to_apng', record.minirecord_to_apng)

setattr(BaseState, 'to_svg', drawing.state_to_svg)
setattr(BaseState, 'to_png', drawing.state_to_png)

setattr(State, 'to_svg', drawing.state_to_svg)
setattr(State, 'to_png', drawing.state_to_png)

setattr(RecordSet, 'save_npz', record.save_record_set)

setattr(OpeningTree, 'save_npz', record.save_opening_tree)
setattr(OpeningTree, 'retrieve_children', record.retrieve_children)
