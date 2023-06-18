from __future__ import annotations
import miniosl
import miniosl.drawing
import numpy as np
import os.path
import copy
import urllib


def is_in_notebook() -> bool:
    """detect run in ipython notebook"""
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
    enhanced with move history
    """
    prefer_svg = False  # shared preference on board representation
    prefer_png = False

    def __init__(self, init: str | miniosl.BaseState | miniosl.MiniRecord = '',
                 *, prefer_png=False, prefer_svg=False, default_format='usi'):
        if not UI.prefer_svg:
            UI.prefer_svg = prefer_svg or is_in_notebook()
        if not UI.prefer_png:
            UI.prefer_png = prefer_png
        self.default_format = default_format

        if isinstance(init, UI):
            self._record = copy.copy(init._record)
            self.replay(init.cur)
            return

        if isinstance(init, miniosl.MiniRecord):
            self._record = copy.copy(init)
        else:
            self._record = miniosl.MiniRecord()
            if init == '':
                self._record.set_initial_state(miniosl.State())  # default
            elif isinstance(init, miniosl.BaseState):
                self._record.set_initial_state(init)
            elif isinstance(init, str):
                if init.startswith('http') and init.endswith('csa'):
                    with urllib.request.urlopen(init) as response:
                        the_csa = response.read().decode('utf-8')
                    self._record = miniosl.csa_record(the_csa)
                elif os.path.isfile(init):
                    if init.endswith('.csa'):
                        self._record = miniosl.csa_record()
                    else:
                        self._record = miniosl.usi_record()
                elif len(init) >= 8:
                    if init[:2] == 'P1':
                        self._record.set_initial_state(miniosl.csa_board(init))
                    else:
                        self._record = miniosl.usi_record(init)
                else:
                    raise ValueError(init+' not expected')
            else:
                raise ValueError(init+' unexpected type')
        self.replay(0)

    def reset(self, src):
        self._record = miniosl.MiniRecord()
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

    def hint(self):
        if UI.prefer_png:
            return self.to_png()
        return self.to_svg() if UI.prefer_svg else self.to_csa()

    # delegation for self._record
    def to_usi_history(self) -> str:
        return self._record.to_usi()

    def previous_repeat_index(self) -> int:
        return self._record.previous_repeat_index(self.cur) if self.cur > 0 else 0

    def repeat_count(self) -> int:
        return self._record.repeat_count(self.cur)

    def to_apng(self, *args, **kwargs):
        return self._record.to_apng(*args, **kwargs)

    # delegation for self._state
    def to_move(self, move_rep):
        return self._state.to_move(move_rep)

    def genmove(self):
        return self._state.genmove()

    def hash_code(self):
        return self._state.hash_code()

    def to_csa(self):
        return self._state.to_csa()

    def to_usi(self):
        return self._state.to_usi()

    def _add_drawing_properties(self, dict):
        dict['last_to'] = self.last_to()
        dict['last_move_ja'] = self.last_move_ja
        dict['move_number'] = self.cur + 1
        dict['repeat_distance'] = self.cur - self.previous_repeat_index()
        dict['repeat_count'] = self.repeat_count()

    def to_svg(self, *args, **kwargs):
        self._add_drawing_properties(kwargs)
        return miniosl.drawing.state_to_svg(self._state, *args, **kwargs)

    def to_png(self, *args, **kwargs):
        self._add_drawing_properties(kwargs)
        return miniosl.drawing.state_to_png(self._state, *args, **kwargs)

    # original / modified methods
    def first(self):
        return self.replay(0)

    def last(self):
        return self.replay(len(self._record))

    def go(self, step):
        if not (0 <= self.cur + step <= len(self._record)):
            raise ValueError(f'step out of range {self.cur} + {step} max {len(self._record)}')
        return self.replay(self.cur+step)

    def make_move(self, move):
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
        return [miniosl.to_ja(move, self._state, miniosl.Square()) for move in self._state.genmove()]

    def last_move(self) -> miniosl.Move | None:
        if not (0 < self.cur <= len(self._record)):
            return None
        return self._record.moves[self.cur-1]

    def last_to(self) -> miniosl.Square | None:
        move = self.last_move()
        return move.dst() if move else None

    def count_hand(self, color: miniosl.Player, ptype: miniosl.Ptype) -> int:
        return self._state.count_hand(color, ptype)

    def turn(self):
        return self._state.turn()

    def copy(self) -> UI:
        copy = UI()
        copy._record = self._record.copy()
        copy._state = self._state.copy()
        copy.cur = self.cur()
        return copy

    def to_np(self) -> np.array:
        return self._state.to_np().reshape(9, 9)

    def to_np_hand(self) -> np.array:
        return self._state.to_np_hand().reshape(2, 7)

    def to_np_cover(self) -> np.array:
        return self._state.to_np_cover().reshape(2, 9, 9)

    def to_np_pack(self) -> np.array:
        return self._state.to_np_pack()

    def replay(self, idx: int):
        self._state = miniosl.State(self._record.initial_state)
        self.cur = idx
        self.last_move_ja = None
        if idx == 0:
            return self.hint()
        if idx > len(self._record):
            raise ValueError(f'index too large {idx} > {len(self._record)}')
        for i, move in enumerate(self._record.moves):
            last_to = None
            if i+1 == idx:
                last_to = self._record.moves[i-1].dst() if i > 0 else miniosl.Square()
            self._do_make_move(move, last_to)
            if i+1 >= idx:
                break
        return self.hint()

    def _do_make_move(self, move: miniosl.Move,
                      last_to: miniosl.Square | None = None):
        if last_to is not None:
            self.last_move_ja = miniosl.to_ja(move, self._state, last_to)
        self._state.make_move(move)
