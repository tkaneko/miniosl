from __future__ import annotations
import minioslcc
import miniosl.drawing
import numpy as np
import random
import drawsvg
import apng


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


class State(minioslcc.CCState):
    """
    shogi state = board position + pieces in hand (mochigoma),
    enhanced with move history
    """
    prefer_svg = False  # shared preference on board representation
    prefer_png = False

    def __init__(self, init='', *, prefer_png=False, prefer_svg=False):
        if isinstance(init, minioslcc.BaseState):
            super().__init__(init)
        else:
            super().__init__()
        self.history = []
        self.last_move_ja = None
        self.id = random.randrange(2**20)
        if not State.prefer_svg:
            State.prefer_svg = prefer_svg or is_in_notebook()
        if not State.prefer_png:
            State.prefer_png = prefer_png
        self.default_format = 'usi'
        if init == '':
            pass                # default
        elif isinstance(init, str) and len(init) >= 8:
            if init[:2] == 'P1':
                super().reset(minioslcc.csa_board(init))
            elif init[0] == 's':
                super().reset(minioslcc.usi_board(init))
            else:
                raise ValueError(init+' not expected')
        elif isinstance(init, minioslcc.MiniRecord):
            super().reset(init.initial_state)
            self.history = init.moves
        self.initial_state = self.to_usi()
        self._replay()

    def reset(self, src):
        super().reset(src)
        self.history = []
        self.last_move_ja = None
        self.initial_state = self.to_usi()

    def __repr__(self) -> str:
        return "<State '" + self.to_usi() + "'>"

    def __str__(self):
        return self.to_csa() if self.default_format == 'csa' else self.to_usi()

    def to_svg(self, *, decorate=False) -> drawsvg.drawing.Drawing:
        return miniosl.drawing.state_to_svg(self, self.id, decorate=decorate)

    def to_png(self, *, decorate=False, filename: str = ''
               ) -> drawsvg.raster.Raster | None:
        png = self.to_svg(decorate=decorate).rasterize()
        return png if not filename else miniosl.drawing.save_png(png, filename)

    def hint(self):
        if State.prefer_png:
            return self.to_png()
        return self.to_svg() if State.prefer_svg else self.to_csa()

    def make_move(self, move):
        if isinstance(move, str):
            copy = move
            move = self.to_move(move)
            if not move.is_normal():
                raise ValueError('please check syntax '+copy)
        if not super().is_legal(move):
            raise ValueError('illegal move '+str(move))
        self.last_move_ja = self.move_to_ja(move)  # need here before make_move
        super().make_move(move)

        self.history.append(move)
        return self.hint()

    def unmake_move(self):
        if len(self.history) < 1:
            raise ValueError('history empty')
        self.history.pop()
        self._replay()
        return self.hint()

    def move_to_ja(self, move: minioslcc.Move) -> str:
        last_to = self.last_to()
        return minioslcc.to_ja(move, self, last_to) if last_to else minioslcc.to_ja(move, self)

    def genmove_ja(self) -> list:
        return [self.move_to_ja(move) for move in self.genmove()]

    def last_move(self) -> minioslcc.Move:
        return self.history[-1] if len(self.history) > 0 else None

    def last_to(self) -> minioslcc.Square:
        move = self.last_move()
        return move.dst() if move else None

    def copy(self) -> State:
        return State(self.to_usi())

    def to_np(self) -> np.array:
        return super().to_np().reshape(9, 9)

    def to_np_hand(self) -> np.array:
        return super().to_np_hand().reshape(2, 7)

    def to_np_cover(self) -> np.array:
        return super().to_np_cover().reshape(2, 9, 9)

    def _replay(self):
        if self.to_usi() != self.initial_state:
            super().reset(minioslcc.usi_board(self.initial_state))
        if len(self.history) == 0:
            return
        self.last_move_ja = None
        for i, move in enumerate(self.history):
            if i == len(self.history)-2:  # just before last make_move
                if i > 0:
                    self.last_move_ja = minioslcc.move_to_ja
                    (self, move, self.history[i-1].to())
                else:
                    self.last_move_ja = minioslcc.move_to_ja(self, move)
            super().make_move(move)
