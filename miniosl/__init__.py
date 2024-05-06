"""miniosl, facade as a Python module.

Many functions are imported from :py:mod:`minioslcc` implemented in C++20.
"""
from minioslcc import *
from .ui import UI
from .record import np_array_to_sfen_file, sfen_file_to_np_array, \
    sfen_file_to_training_np_array, load_record_set, load_opening_tree, \
    SfenBlockStat, load_ki2, read_ki2
from .drawing import show_channels, state_to_png, state_to_svg, to_png_bytes, \
    ptype_to_ja, hand_pieces_to_ja
from .dataset import load_torch_dataset, GameDataset
from .network import StandardNetwork
from .usi_process import UsiProcess
from .player import UsiPlayer, make_player
from .inference import export_model, softmax, InferenceModel, p2elo
from .search import run_mcts


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


def version():
    import importlib.metadata
    return importlib.metadata.version('miniosl')


def pretrained_eval_path():
    import importlib.resources
    return importlib.resources.files('miniosl').joinpath('pretrained/eval.onnx')


def has_pretrained_eval():
    import os.path
    return os.path.exists(pretrained_eval_path())


def install_coloredlogs(level: str = 'INFO'):
    import coloredlogs
    fmt = '%(asctime)s %(hostname)s %(levelname)s %(message)s'
    field_styles = {
        'asctime': {'color': 96, 'background': 'white'},
        'hostname': {'color': 39},
        'levelname': {'color': 247},
    }
    coloredlogs.install(level=level, fmt=fmt, field_styles=field_styles)

# flake8: noqa
