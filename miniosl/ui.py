"""primary user interface of miniosl"""
from __future__ import annotations
import miniosl
from minioslcc import MiniRecord, BaseState, State, Square, Move
import miniosl.drawing
import numpy as np
import os.path
import copy
import urllib
import logging
import math
from typing import Tuple

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
    general interface for \
    shogi state (:py:class:`State` , board position + pieces in hand),
    enhanced with move history (:py:class:`MiniRecord`) and other utilities

    :param init: initial contents handled by :py:meth:`load_record`
    :param prefer_png: preference of state visualization in notebook
    :param prefer_svg: preference of state visualization in notebook
    :param default_format: preference in `__str__`

    make the initial state in shogi

    >>> shogi = miniosl.UI()

    or read csa/usi file (local or web)

    >>> url = 'http://live4.computer-shogi.org/wcsc33/kifu/WCSC33+F7_1-900-5F+dlshogi+Ryfamate+20230505161013.csa'
    >>> shogi = miniosl.UI(url)
    >>> _ = shogi.go(10)
    >>> print(shogi.to_csa(), end='')
    P1-KY-KE * -KI *  * -GI-KE-KY
    P2 * -HI-GI * -OU * -KI-KA * 
    P3-FU * -FU-FU-FU-FU-FU-FU-FU
    P4 *  *  *  *  *  *  *  *  * 
    P5 * -FU *  *  *  *  * +FU * 
    P6+FU *  *  *  *  *  *  *  * 
    P7 * +FU+FU+FU+FU+FU+FU * +FU
    P8 * +KA+KI *  *  * +GI+HI * 
    P9+KY+KE+GI * +OU+KI * +KE+KY
    +
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
        self.model = None

        if isinstance(init, UI):
            self._record = copy.copy(init._record)
            self.replay(init.cur)
            return
        self.load_record(init)

    def load_record_set(self, path: str, idx: int):
        """load idx-th game record in :py:class:`RecordSet`

        :param path: filepath for `.npz` generated by \
        :py:meth:`RecordSet.save_npz` or sfen text
        """
        if path.endswith('.npz'):
            set = miniosl.RecordSet.from_npz(path, limit=idx+1)
        else:
            set = miniosl.RecordSet.from_usi_file(path)
        if idx >= len(set.records):
            raise ValueError(f'idx {idx} >= len {len(set.records)}'
                             + f' of record set {str}')
        self.load_record(set.records[idx])

    def load_record(self, src: str | BaseState | MiniRecord = ''):
        """load a game record from various sources.

        :param src: :py:class:`BaseState` or :py:class:`MiniRecord` \
        or URL or filepath containing `.csa` or usi.
        """
        self._record = None
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
        return self.to_svg(*args, **kwargs) if UI.prefer_svg else self.to_usi()

    # delegation for self._record
    def to_usi_history(self) -> str:
        """show the history in usi

        >>> shogi = miniosl.UI()
        >>> _ = shogi.make_move('+2726FU')
        >>> _ = shogi.make_move('-3334FU')
        >>> shogi.to_usi()
        'sfen lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/7P1/PPPPPPP1P/1B5R1/LNSGKGSNL b - 1'
        >>> shogi.to_usi_history()
        'startpos moves 2g2f 3c3d'
        """
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
                           last_to: Square | None = None) -> Move:
        return self._state.read_japanese_move(move_rep, last_to or Square())

    def genmove(self):
        """generate legal moves in the state"""
        return self._state.genmove()

    def hash_code(self):
        return self._state.hash_code()

    def to_csa(self) -> str:
        """show state in csa"""
        return self._state.to_csa()

    def to_usi(self) -> str:
        """show state in usi"""
        return self._state.to_usi()

    def _add_drawing_properties(self, dict):
        dict['last_to'] = self.last_to()
        dict['last_move_ja'] = self.last_move_ja
        dict['move_number'] = self.cur + 1
        dict['repeat_distance'] = self.cur - self.previous_repeat_index()
        dict['repeat_count'] = self.repeat_count()

    def to_svg(self, *args, **kwargs):
        """show state in svg

        parameters will be passed to :py:func:`state_to_svg`

        :return: svg image to be shown in colab or jupyter notebooks
        """
        self._add_drawing_properties(kwargs)
        return miniosl.state_to_svg(self._state, *args, **kwargs)

    def to_png(self, *args, **kwargs):
        """show state in png

        parameters will be passed to :py:func:`state_to_png`

        :return: png image to be shown in colab or jupyter notebooks
        """
        self._add_drawing_properties(kwargs)
        return miniosl.state_to_png(self._state, *args, **kwargs)

    # original / modified methods
    def first(self):
        """go to the first state in the history"""
        return self.replay(0)

    def last(self):
        """go to the last state in the history"""
        return self.replay(len(self._record))

    def last_move_number(self):
        return len(self._record)

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
            self._record.append_move(move, self._state.in_check())
            self._record.settle_repetition()  # better to improve performance
        self.cur += 1
        return self.hint()

    def unmake_move(self):
        """undo the last move"""
        if self.cur < 1:
            raise ValueError('history empty')
        return self.replay(self.cur-1)

    def genmove_ja(self) -> list[str]:
        """generate legal moves in Japanese"""
        return [miniosl.to_ja(move, self._state, Square())
                for move in self._state.genmove()]

    def last_move(self) -> Move | None:
        """the last move played or None"""
        if not (0 < self.cur <= len(self._record)):
            return None
        return self._record.moves[self.cur-1]

    def last_to(self) -> Square | None:
        """the destination square of the last move or None

        >>> shogi = miniosl.UI()
        >>> shogi.last_to()
        >>> _ = shogi.make_move('+2726FU')
        >>> shogi.last_to() == miniosl.Square(2, 6)
        True
        """
        move = self.last_move()
        return move.dst() if move else None

    def count_hand(self, color: miniosl.Player, ptype: miniosl.Ptype) -> int:
        """number of pieces in hand"""
        return self._state.count_hand(color, ptype)

    def turn(self) -> miniosl.Player:
        """side to move"""
        return self._state.turn()

    def to_np_state_feature(self) -> np.array:
        """make tensor of feature for the current state (w/o history).

        - 44ch each 9x9 channel is responsible \
          for a specific piece type and color
          - where each element is 1 (0) for existence (absent) of the piece \
            for the first 30ch
          - filled by the same value indicating number of hand pieces \
            for the latter 14ch
        - 13ch for heuristic features
        """
        return self._state.to_np_state_feature()

    def to_np_cover(self) -> np.array:
        """make planes to show a square is covered (1) or not (0)"""
        return self._state.to_np_cover()

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
        logging.info(f'load opening of size {self.opening_tree.size()}')

    def opening_moves(self):
        """retrieve or show opening moves.

        Note: need to load data by :py:meth:`load_opening_tree` in advance
        """
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
            history = self._record.moves[:self.cur]
            f = self._record.initial_state.export_features(history)
            self._features = f

    def show_channels(self, *args, **kwargs):
        if not hasattr(UI, 'japanese_available_in_plt'):
            fontname = 'Noto Sans CJK JP'
            import matplotlib.font_manager as fm
            font = fm.findfont(fontname)
            UI.japanese_available_in_plt = font and ("NotoSansCJK" in font)
            if UI.japanese_available_in_plt:
                import matplotlib
                matplotlib.rcParams['font.family'] = [fontname]
        return miniosl.show_channels(*args, **kwargs,
                                     japanese=UI.japanese_available_in_plt)

    def show_features(self, plane_id: int | str, *args, **kwargs):
        """visualize features in matplotlib.

        :param plane_id: ``'pieces'`` | ``'hands'`` | ``'lastmove'`` \
        | ``'long'`` | ``'safety'``, or indeger id (in internal representation)
        """
        self.update_features()
        turn = self._state.turn()
        flip = turn == miniosl.white
        if isinstance(plane_id, str):
            if plane_id == "pieces":
                print('side-to-move PLNSBRk/gplnsbr')
                self.show_channels(self._features[16:], 2, 7, flip)
                print('opponent PLNSBRk/gplnsbr')
                self.show_channels(self._features, 2, 7, flip)
                return
            if plane_id == "hands":
                print('plnsgbr')
                return self.show_channels(self._features[30:], 2, 7, flip)
            if plane_id == "long":
                print('lbr+k')
                return self.show_channels(self._features[44:], 4, 4, flip)
            if plane_id == "safety":
                return self.show_channels(self._features[61:], 1, 3, flip)
            if plane_id == "safety_1":
                id = miniosl.channel_id['check_piece_1']
                return self.show_channels(self._features[id:], 1, 6, flip)
            if plane_id == "safety_2":
                id = miniosl.channel_id['check_piece_2']
                return self.show_channels(self._features[id:], 1, 6, flip)
            latest_history = 64
            if plane_id == "lastmove":
                return self.show_channels(self._features[latest_history:], 1, 3, flip)
            plane_id = miniosl.channel_id[plane_id]
        if not is_in_notebook():
            print(self._features[plane_id])
        return self.hint(plane=self._features[plane_id], flip_if_white=True,
                         *args, **kwargs)

    def load_eval(self, path: str, device: str = "", torch_cfg: dict = {}):
        """load parameters of evaluation function from file

        parameters will be passed to :py:func:`inference.load`.
        """
        import miniosl.inference
        path = os.path.expanduser(path)
        self.model = miniosl.inference.load(path, device, torch_cfg)

    def eval(self, verbose=True) -> Tuple[float, list]:
        """return value and policy for current state.

        need to call :py:meth:`load_eval` in advance
        """
        self.update_features()
        logits, value, aux = self.model.eval(self._features,
                                             take_softmax=False)
        policy = miniosl.softmax(logits.reshape(-1)).reshape(-1, 9, 9)
        flip = self._state.turn() == miniosl.white
        if verbose:
            self.show_channels([np.max(policy, axis=0)], 1, 1, flip)
            print(f'eval = {value*Value_Scale:.0f}')
        mp = miniosl.inference.sort_moves(self.genmove(), policy)
        self._infer_result = {'policy': policy, 'value': value, 'mp': mp,
                              'aux': aux, 'logits': logits}
        if verbose:
            for i in range(min(len(mp), 3)):
                print(mp[i][1], f'{mp[i][0]*100:6.1f}%',
                      miniosl.to_ja(mp[i][1], self._state))
        return value*Value_Scale, mp

    def gumbel_one(self, width: int = 4):
        if self._features is None or self._infer_result is None:
            self.eval(verbose=False)
        history = self._record.moves[:self.cur]
        mp = self._infer_result['mp']
        logits = self._infer_result['logits'].reshape(-1)
        values = []
        for i in range(min(len(mp), width)):
            move = mp[i][1]
            f, terminal = self._record.initial_state.export_features_after_move(history, move)
            _, v, _ = self.model.infer_one(f)
            logit = logits[move.policy_move_label()]
            v = -v.item()       # negamax
            if terminal == miniosl.win_result(self._state.turn()):
                v = 1.0
            elif terminal == miniosl.win_result(miniosl.alt(self._state.turn())):
                v = -1.0
            elif terminal == miniosl.Draw:
                v = 0
            values.append([logit + miniosl.transformQ(v), move, logit, v])
        values.sort(key=lambda e: -e[0])
        self._infer_result[f'gumbel{width}'] = values
        return values

    def mcts(self, budget: int = 4):
        logging.info(f'ui.record {self._record.initial_state}')
        record = self._record
        if self.cur < record.move_size():
            record = record.branch_at(self.cur)
        mgr = miniosl.GameManager.from_record(record)
        logging.info(f'mgr.state {mgr.state}')
        # logging.info(f'root {mgr.legal_moves}')
        root = miniosl.run_mcts(mgr, budget, self.model)
        clist = sorted(root.children.items(), key=lambda e: -e[1].visit_count)
        for move, c in clist:
            if c.visit_count > math.log(budget)-1:
                print(f'{move.to_csa()}, {c.policy_probability*100:.1f}%,'
                      + f' {1-c.value():.2f}, {c.visit_count}')
        return root

    def analyze(self):
        """evaluate all positions in the current game"""
        import matplotlib.pyplot as plt
        now = self.cur
        self.replay(0)
        evals = []
        for i in range(self._record.state_size()):
            if i > 0:
                self._do_make_move(self._record.moves[i-1])
            self.cur = i
            value, *_ = self.eval(False)
            evals.append(value * miniosl.sign(self._state.turn()))
        self.replay(now)
        plt.axhline(y=0, linestyle='dotted')
        return plt.plot(np.array(evals), '+')

    def show_inference_after_move(self):
        if self._features is None or self._infer_result is None:
            return
        return self.show_channels(self._infer_result['aux'], 2, 6)

    def ipywidget(self):
        import ipywidgets
        image = ipywidgets.Image(
            value=miniosl.to_png_bytes(self.to_png()),
            format='png',
            width=290,
            height=250,
        )
        shogi_range = ipywidgets.IntSlider(
            min=0,
            max=len(self._record),
            value=self.cur,
            description='Move number:',
        )

        def shogi_value_change(change):
            id = int(change['new'])
            self.replay(id)
            image.value = miniosl.to_png_bytes(self.to_png())

        shogi_range.observe(shogi_value_change, names='value')
        box = ipywidgets.VBox([image, shogi_range])
        return box
