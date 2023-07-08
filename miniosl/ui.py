from __future__ import annotations
import miniosl
from minioslcc import MiniRecord, BaseState, State, Square, Move
import miniosl.drawing
import numpy as np
import os.path
import copy
import urllib

Value_Scale = 1000


def is_in_notebook() -> bool:
    """detect run inside ipython notebook"""
    # https://stackoverflow.com/questions/15411967/
    try:
        shell = get_ipython().__class__
        if 'google.colab' in str(shell):
            return True
        shell = shell.__name__
        if shell == 'ZMQInteractiveShell':
            return True   # Jupyter notebook or qtconsole
        elif shell == 'TerminalInteractiveShell':
            return False  # Terminal running IPython
        else:
            return False  # Other type (?)
    except NameError:
        return False      # Probably standard Python interpreter


class UI:
    """
    shogi state = board position + pieces in hand (mochigoma),
    enhanced with move history and other utilities
    """
    prefer_svg = False  # shared preference on board representation
    prefer_png = False

    def __init__(self, init: str | BaseState | MiniRecord = '',
                 *, prefer_png=False, prefer_svg=False, default_format='usi'):
        if not UI.prefer_svg:
            UI.prefer_svg = prefer_svg or is_in_notebook()
        if not UI.prefer_png:
            UI.prefer_png = prefer_png
        self.default_format = default_format
        self.opening_tree = None
        self.nn = None
        self._features = None
        self._infer_result = None

        if isinstance(init, UI):
            self._record = copy.copy(init._record)
            self.replay(init.cur)
            return
        self.load_record(init)

    def load_record_set(self, path: str, idx: int):
        if path.endswith('.npz'):
            set = miniosl.RecordSet.from_npz(path, limit=idx+1)
        else:
            set = miniosl.RecordSet.from_usi_file(path)
        if idx >= len(set.records):
            raise ValueError(f'idx {idx} >= len {len(set.records)}'
                             + f' of record set {str}')
        self.load_record(set.records[idx])

    def load_record(self, src: str | BaseState | MiniRecord = ''):
        self._record = MiniRecord()
        if isinstance(src, MiniRecord):
            self._record = copy.copy(src)
        else:
            self._record = MiniRecord()
            if src == '':
                self._record.set_initial_state(State())  # default
            elif isinstance(src, BaseState):
                self._record.set_initial_state(src)
            elif isinstance(src, str):
                if src.startswith('http') and src.endswith('csa'):
                    with urllib.request.urlopen(src) as response:
                        the_csa = response.read().decode('utf-8')
                    self._record = miniosl.csa_record(the_csa)
                elif os.path.isfile(src):
                    if src.endswith('.csa'):
                        self._record = miniosl.csa_record()
                    else:
                        self._record = miniosl.usi_record()
                elif len(src) >= 8:
                    if src[:2] == 'P1':
                        self._record.set_initial_state(miniosl.csa_board(src))
                    else:
                        self._record = miniosl.usi_record(src)
                else:
                    raise ValueError(src+' not expected')
            else:
                raise ValueError(src+' unexpected type')
        return self.replay(0)

    def __repr__(self) -> str:
        return "<UI '" + self.to_usi_history() + "'>"

    def __str__(self):
        return self._state.to_csa() if self.default_format == 'csa' else self._state.to_usi()

    def __copy__(self):
        return UI(self)

    def __deepcopy__(self, dict):
        return UI(self)

    def __len__(self) -> int:
        return self._record.state_size()  # state-size, that is, move-size + 1

    def hint(self, *args, **kwargs):
        if UI.prefer_png:
            return self.to_png(*args, **kwargs)
        return self.to_svg(*args, **kwargs) if UI.prefer_svg else self.to_csa()

    # delegation for self._record
    def to_usi_history(self) -> str:
        """show the history in usi"""
        return self._record.to_usi()

    def previous_repeat_index(self) -> int:
        """the latest repeating state the history"""
        return self._record.previous_repeat_index(self.cur) if self.cur > 0 else 0

    def repeat_count(self) -> int:
        """number of occurrence of the state in the history"""
        return self._record.repeat_count(self.cur)

    def to_apng(self, *args, **kwargs):
        """make animated png for the history"""
        return self._record.to_apng(*args, **kwargs)

    # delegation for self._state
    def to_move(self, move_rep: str) -> Move:
        """interpret string as a Move"""
        return self._state.to_move(move_rep)

    def read_japanese_move(self, move_rep: str,
                           last_to: Square | None = None):
        return self._state.read_japanese_move(move_rep, last_to or Square())

    def genmove(self):
        """generate legal moves in the state"""
        return self._state.genmove()

    def hash_code(self):
        return self._state.hash_code()

    def to_csa(self):
        """show state in csa"""
        return self._state.to_csa()

    def to_usi(self):
        """show state in usi"""
        return self._state.to_usi()

    def _add_drawing_properties(self, dict):
        dict['last_to'] = self.last_to()
        dict['last_move_ja'] = self.last_move_ja
        dict['move_number'] = self.cur + 1
        dict['repeat_distance'] = self.cur - self.previous_repeat_index()
        dict['repeat_count'] = self.repeat_count()

    def to_svg(self, *args, **kwargs):
        """show state in svg"""
        self._add_drawing_properties(kwargs)
        return miniosl.state_to_svg(self._state, *args, **kwargs)

    def to_png(self, *args, **kwargs):
        """show state in png"""
        self._add_drawing_properties(kwargs)
        return miniosl.state_to_png(self._state, *args, **kwargs)

    # original / modified methods
    def first(self):
        """go to the first state in the history"""
        return self.replay(0)

    def last(self):
        """go to the last state in the history"""
        return self.replay(len(self._record))

    def go(self, step):
        """make moves (step > 0) or unmake moves (step < 0)"""
        if not (0 <= self.cur + step <= len(self._record)):
            raise ValueError(f'step out of range {self.cur}'
                             + f' + {step} max {len(self._record)}')
        return self.replay(self.cur+step)

    def make_move(self, move):
        """make a move in the current state"""
        if isinstance(move, str):
            copy = move
            move = self._state.to_move(move)
            if not move.is_normal():
                raise ValueError('please check syntax '+copy)
        if not self._state.is_legal(move):
            raise ValueError('illegal move '+str(move))
        if self.cur < len(self._record) and self._record.moves[self.cur] == move:
            self._do_make_move(move, self.last_to())
        else:
            if self.cur < len(self._record):
                self._record = self._record.branch_at(self.cur)
            self._do_make_move(move, self.last_to())
            self._record.add_move(move, self._state.in_check())
            self._record.settle_repetition()  # better to improve performance
        self.cur += 1
        return self.hint()

    def unmake_move(self):
        if self.cur < 1:
            raise ValueError('history empty')
        return self.replay(self.cur-1)

    def genmove_ja(self) -> list:
        return [miniosl.to_ja(move, self._state, Square())
                for move in self._state.genmove()]

    def last_move(self) -> Move | None:
        if not (0 < self.cur <= len(self._record)):
            return None
        return self._record.moves[self.cur-1]

    def last_to(self) -> Square | None:
        move = self.last_move()
        return move.dst() if move else None

    def count_hand(self, color: miniosl.Player, ptype: miniosl.Ptype) -> int:
        return self._state.count_hand(color, ptype)

    def turn(self):
        return self._state.turn()

    def to_np(self) -> np.array:
        """make tensor of piece placement in the board.
           each 9x9 channel is responsible for a specific piece type and color,
           where each element is 1 (0) for existence (absent) of the piece
        """
        return self._state.to_np().reshape(9, 9)

    def to_np_hand(self) -> np.array:
        """make tensor of pieces in hand for both player"""
        return self._state.to_np_hand().reshape(2, 7)

    def to_np_cover(self) -> np.array:
        """make planes to show a square is covered (1) or not (0)"""
        return self._state.to_np_cover().reshape(2, 9, 9)

    def to_np_pack(self) -> np.array:
        """compress state information"""
        return self._state.to_np_pack()

    def replay(self, idx: int):
        """move to idx-th state in the history"""
        self._state = State(self._record.initial_state)
        self._features = None
        self.cur = idx
        self.last_move_ja = None
        if idx == 0:
            return self.hint()
        if idx > len(self._record):
            raise ValueError(f'index too large {idx} > {len(self._record)}')
        for i, move in enumerate(self._record.moves):
            last_to = None
            if i+1 == idx:
                last_to = self._record.moves[i-1].dst() if i > 0 else Square()
            self._do_make_move(move, last_to)
            if i+1 >= idx:
                break
        return self.hint()

    def _do_make_move(self, move: Move,
                      last_to: Square | None = None):
        self._features = None
        if last_to is not None:
            self.last_move_ja = miniosl.to_ja(move, self._state, last_to)
        else:
            self.last_move_ja = miniosl.to_ja(move, self._state)
        self._state.make_move(move)

    def load_opening_tree(self, filename):
        """load opening db"""
        self.opening_tree = miniosl.load_opening_tree(filename)

    def opening_moves(self):
        """retrieve or show opening moves"""
        if self.opening_tree is None:
            raise RuntimeError("opening_tree not loaded")
        all = self.opening_tree[self._state.hash_code()].count()
        children = self.opening_tree.retrieve_children(self._state)
        if not is_in_notebook():
            for c in children:
                print(c[1], f'{c[0].count()/all*100:5.2f}% {c[0].count():5}'
                      + f'({c[0].black_advantage()*100:5.2f}%)', )
        else:
            plane = np.zeros((9, 9))
            for c in children:
                x, y = c[1].dst().to_xy()
                plane[y-1][x-1] = c[0].count()/all
            return self.hint(plane=plane)

    def follow_opening(self, nth: int = 0):
        """make a move following the opening db"""
        if self.opening_tree is None:
            raise RuntimeError("opening_tree not loaded")
        children = self.opening_tree.retrieve_children(self._state)
        if nth >= len(children):
            raise IndexError(f"opening {nth} >= {len(children)}")
        self.make_move(children[nth][1])
        return self.opening_moves()

    def make_opening_db_from_sfen(self, filename: str, threshold: int = 100):
        """make opening tree from sfen records, and save in npz"""
        set = miniosl.RecordSet.from_usi_file(filename)
        if not filename.endswith('.npz'):
            set.save_npz("sfen.npz")
        tree = miniosl.OpeningTree.from_record_set(set, threshold)
        self.opening_tree = tree
        tree.save_npz('opening.npz')

    def update_features(self):
        if self._features is None:
            if last_move := self.last_move():
                f = miniosl.export_heuristic_feature(self._state, last_move)
            else:
                f = miniosl.export_heuristic_feature(self._state)
            self._features = f.reshape((-1, 9, 9))

    def show_features(self, plane_id: int | str, *args, **kwargs):
        self.update_features()
        turn = self._state.turn()
        flip = turn == miniosl.white
        if isinstance(plane_id, str):
            if plane_id == "pieces":
                print('side-to-move PLNSBRk/gplnsbr')
                miniosl.drawing.show_channels(self._features[16:], 2, 7, flip)
                print('opponent PLNSBRk/gplnsbr')
                miniosl.drawing.show_channels(self._features, 2, 7, flip)
                return
            if plane_id == "hands":
                print('plnsgbr')
                return miniosl.show_channels(self._features[30:], 2, 7, flip)
            if plane_id == "long":
                print('lbr+k')
                return miniosl.show_channels(self._features[44:], 2, 4, flip)
            if plane_id == "safety":
                return miniosl.show_channels(self._features[52:], 1, 2, flip)
            if plane_id == "lastmove":
                return miniosl.show_channels(self._features[57:], 1, 3, flip)
            plane_id = miniosl.channel_id[plane_id]
        if not is_in_notebook():
            print(self._features[plane_id])
        return self.hint(plane=self._features[plane_id], flip_if_white=True,
                         *args, **kwargs)

    def load_eval(self, path: str, device: str = "", torch_cfg: dict = {}):
        import miniosl.inference
        path = os.path.expanduser(path)
        self.model = miniosl.inference.load(path, device, torch_cfg)

    def eval(self, verbose=True):
        self.update_features()
        policy, value, aux = self.model.eval(self._features)
        flip = self._state.turn() == miniosl.white
        if verbose:
            miniosl.show_channels([np.max(policy, axis=0)], 1, 1, flip)
            print(f'eval = {value*Value_Scale:.0f}')
        mp = miniosl.inference.sort_moves(self.genmove(), policy)
        self._infer_result = (policy, value, mp, aux)
        if verbose:
            for i in range(min(len(mp), 3)):
                print(mp[i][1], f'{mp[i][0]*100:6.1f}%')
        return value*Value_Scale, mp

    def analyze(self):
        import matplotlib.pyplot as plt
        now = self.cur
        self.replay(0)
        evals = []
        for i in range(self._record.state_size()):
            if i > 0:
                self._do_make_move(self._record.moves[i-1])
            value, *_ = self.eval(False)
            evals.append(value * miniosl.sign(self._state.turn()))
        self.replay(now)
        plt.axhline(y=0, linestyle='dotted')
        return plt.plot(np.array(evals), '+')

    def show_inference_after_move(self):
        if self._features is None or self._infer_result is None:
            return
        return miniosl.show_channels(self._infer_result[2], 2, 6)
