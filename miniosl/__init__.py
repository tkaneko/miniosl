"""miniosl, facade as a Python module.

Many functions are imported from :py:mod:`minioslcc` implemented in C++20.
"""
from minioslcc import *
from miniosl.ui import UI
from .record import np_array_to_sfen_file, sfen_file_to_np_array, \
    sfen_file_to_training_np_array, load_record_set, load_opening_tree, \
    SfenBlockStat
from .drawing import show_channels, state_to_png, state_to_svg, to_png_bytes, \
    ptype_to_ja, hand_pieces_to_ja
from .dataset import load_torch_dataset, PositionwiseDataset, GameDataset
from .network import StandardNetwork
from .usi_process import UsiProcess
from .player import UsiPlayer, make_player
from .inference import export_model, softmax

setattr(MiniRecord, 'replay', record.minirecord_replay)
setattr(MiniRecord, 'to_apng', record.minirecord_to_apng)

setattr(SubRecord, 'replay', record.subrecord_replay)

setattr(BaseState, 'to_svg', state_to_svg)
setattr(BaseState, 'to_png', state_to_png)

setattr(State, 'to_svg', state_to_svg)
setattr(State, 'to_png', state_to_png)

setattr(RecordSet, 'save_npz', record.save_record_set)
setattr(RecordSet, 'from_npz', record.load_record_set)

setattr(OpeningTree, 'save_npz', record.save_opening_tree)
setattr(OpeningTree, 'retrieve_children', record.retrieve_children)
# flake8: noqa
