"""visualize state and channels"""
from __future__ import annotations
import miniosl
import numpy as np
import matplotlib
import matplotlib.pyplot as plt
import logging


font_initialized = False
properties = [
    {
        "heps": +0.02, "veps": +0.1,
        "verticalalignment": 'center',
        "zorder": 2,
        "rotation": 0,
    },
    {
        "heps": +0.04, "veps": -0.15,
        "verticalalignment": 'center',
        "horizontalalignment": 'center',
        "rotation_mode": 'anchor',
        "zorder": 2,
        "rotation": 180,
    }
]
last_move_prop = {
    "x": 6.25, "y": 10,
    "size": 10, "alpha": 0.9, "horizontalalignment": 'left'
}


def setup_font():
    """look for Noto fonts for Japanese characters"""
    # might need addfont in Colab
    # https://matplotlib.org/stable/api/font_manager_api.html#matplotlib.font_manager.FontManager.addfont
    import matplotlib.font_manager as fm
    # add system fonts in advance, especially for colab
    font_dirs = ["/usr/share/fonts"]
    font_files = fm.findSystemFonts(fontpaths=font_dirs)
    for file in font_files:
        if 'JP' in file or 'CJK' in file:
            fm.fontManager.addfont(file)

    fontname = 'Noto Serif CJK JP'  # 'Noto Sans Mono CJK JP'
    font = fm.findfont(fontname)
    if not font and ("NotoSerifCJK" in font):  # 'NotoSansMonoCJKjp'
        logging.warning(f'{fontname} not available')
    matplotlib.rcParams['font.family'] = [fontname]


def place_letter(ax, x: int, y: int, label: str,
                 heps: float, veps: float,
                 **kwargs):
    return ax.text(x+heps, y+veps, label, **kwargs)


def update_letter(text, x: int, y: int, label: str,
                  *,
                  heps: float, veps: float,
                  **kwargs):
    text.set(position=(x+heps, y+veps), text=label, **kwargs)


def is_outside(ax, x: int, y: int):
    xlim = int(np.floor(ax.get_xlim()[0]))
    if x >= xlim:
        return True
    ylim = int(np.floor(ax.get_ylim()[0]))
    if y >= ylim:
        return True
    return False


def put_forward_char(ax, x: int, y: int, label: str,
                     size: int = 15, alpha: float = 1,
                     horizontalalignment: str = 'center',
                     allow_outside: bool = False):
    """place a piece label at (x, y) if in [1,9]^2 or outside otherwise

    :param ax: object returned by make_board_fig()
    """
    if is_outside(ax, x, y) and not allow_outside:
        return None
    return place_letter(ax, x, y, label, fontsize=size,
                        alpha=alpha,
                        horizontalalignment=horizontalalignment,
                        **properties[miniosl.black])


def put_reversed_char(ax, x: int, y: int, label: str,
                      size: int = 15):
    """place a piece label at (x, y) in [1,9]^2 with rotation

    :param ax: object returned by make_board_fig()
    """
    if is_outside(ax, x, y):
        return None
    return place_letter(ax, x, y, label, fontsize=size,
                        **properties[miniosl.white])


def place_black_hand_piece(ax, pieces: str = '', offset: int = 0):
    """place a sequence of labels in black piece stand

    :param ax: object returned by make_board_fig()
    :param pieces: a sequence of piece letters typically in Japanese
    :param offset: number of protected pieces already written
    """
    lst = []
    for i, char in enumerate(pieces):
        n = i + offset
        c, r = n // 12, n % 12
        p = put_forward_char(ax, -1.95+c*0.85, 1.5+r*0.8, char, size=13)
        lst.append(p)
    return lst


def place_white_hand_piece(ax, pieces: str = '', offset: int = 0):
    """place a sequence of labels (with rotation) in white piece stand

    :param ax: object returned by make_board_fig()
    :param pieces: a sequence of piece letters typically in Japanese
    :param offset: number of protected pieces already written
    """
    lst = []
    for i, char in enumerate(pieces):
        n = i + offset
        c, r = n // 12, n % 12
        p = put_reversed_char(ax, 11.4-c*0.85, 8.5-r*0.8, char, size=13)
        lst.append(p)
    return lst


default_xlim, default_ylim = 12, 12
xmin, ymin = -2.5, -.5


def xy_ratio(xlim, ylim):
    xratio = (xlim - xmin) / (default_xlim - xmin)
    yratio = (ylim - ymin) / (default_ylim - ymin)
    return (xratio, yratio)


def make_board_fig(id: int | None = None, *, xlim=None, ylim=None) -> tuple[
        matplotlib.figure.Figure, matplotlib.axes._axes.Axes
]:
    """make a matplotlib.fig (and ax) for an empty board

    :param id: apparently obsolete in recent matplotlib
    """
    xlim, ylim = xlim or default_xlim, ylim or default_ylim
    xratio, yratio = 1, 1  # xy_ratio(xlim, ylim)
    fig, ax = plt.subplots(
        figsize=[3.3*xratio, 3.3*yratio],
        # num=id, clear=True
    )
    color = matplotlib.rcParams['grid.color']  # respect current theme
    for i in range(10):
        alpha, lw = (1, 1) if i in [0, 9] else (0.45, 0.8)
        ax.plot([i+.5, i+.5], [0.5, 9.5],
                alpha=alpha, linewidth=lw, color=color)  # vert
        ax.plot([0.5, 9.5], [i+.5, i+.5],
                alpha=alpha, linewidth=lw, color=color)  # horiz
    ax.set_position([0, 0, 1, 1])
    ax.set_axis_off()
    ax.set_xlim(xmin, xlim)
    ax.set_ylim(ymin, ylim)
    ax.invert_xaxis()
    ax.invert_yaxis()
    for x in range(1, 10):
        put_forward_char(ax, x, 0, str(x), size=10, alpha=0.7)
    for y, char in enumerate(['一', '二', '三', '四', '五', '六', '七', '八', '九']):
        put_forward_char(ax, 0, y+1, char, size=10, alpha=0.7)
    put_forward_char(ax, -2, 0.5, '☗')
    put_reversed_char(ax, 11.5, 9.5, '☖')
    return fig, ax


def hv_offset(ax) -> tuple[int, int]:
    hoffset = 0
    xlim = ax.get_xlim()[0]
    if xlim < default_xlim:
        hoffset -= default_xlim - xlim - 3
    voffset = 0
    ylim = ax.get_ylim()[0]
    if ylim < default_ylim:
        voffset -= default_ylim - ylim - 2
    return (hoffset, voffset)


def add_move_number(ax, msg: str, hoffset: int = 0):
    """add msg at the bottom

    :param ax: object returned by make_board_fig()
    """
    hoffset, voffset = hv_offset(ax)
    return put_forward_char(ax, 9.25 + hoffset, 10.75 + voffset, msg,
                            size=10, alpha=.9,
                            horizontalalignment='left',
                            allow_outside=True)


def add_last_move(ax, last_move: str):
    """show last_move at a line above the bottom

    :param ax: object returned by make_board_fig()
    """
    msg = f'({last_move} まで)' if last_move else ''
    prop = dict(**last_move_prop)
    hoffset, voffset = hv_offset(ax)
    prop['x'] += hoffset
    prop['y'] += voffset
    return put_forward_char(ax, label=msg, allow_outside=True, **prop)


kanjirow = [None, '一', '二', '三', '四', '五', '六', '七', '八', '九']
kanjicol = [None, '1', '2', '3', '4', '5', '6', '7', '8', '9']

no_pieces_in_hand = 'なし'
label_hand_b = '先手持駒'
label_hand_w = '後手持駒'
hand_others = '他'


def hand_pieces_to_ja(state, player):
    ret = ''
    for ptype in miniosl.piece_stand_order:
        cnt = state.count_hand(player, ptype)
        if cnt == 0:
            continue
        ret += ptype.to_ja1() * cnt
    return ret


def radius(cnt):
    """radius of circle for strength cnt"""
    return 12 * (cnt ** 1/4)


class ShogiFig:
    """data holding state and matplotlib objects

    use `self.fig` for completed image.
    """
    def __init__(self, state: miniosl.BaseState, last_move_ja: str = '',
                 move_number: int = 0, id: int = 4081, xlim=None, ylim=None):
        global font_initialized
        if not font_initialized:
            setup_font()
        self.state = miniosl.State(state)  # clone
        self.last_move_ja = last_move_ja
        self.last_to = None
        self.move_number = move_number
        self.fig, self.ax = make_board_fig(id=id, xlim=xlim, ylim=ylim)
        self.arts = {}
        self.place_pieces()
        self._init_hand()
        self.floating_piece = None  # intermediate flames in dropping pieces
        plt.close()

    def _decorate_square(self, x: int, y: int, radius: float,
                         color: str, marker: str = 'o',
                         markeredgewidth: float = 1,
                         alpha: float = .5) -> None:
        return self.ax.plot(
            x, y, marker, color=color,
            markersize=radius,
            markeredgewidth=markeredgewidth,
            alpha=alpha, zorder=1)

    def _decorate_king(self, player: miniosl.Player) -> None:
        x, y = self.state.king_square(player).to_xy()
        self._decorate_square(x, y, 16, 'C2', marker='s',
                              markeredgewidth=2, alpha=.2)

    def decorate_cover(self):
        if not isinstance(self.state, miniosl.State):
            return
        self._decorate_king(miniosl.black)
        self._decorate_king(miniosl.white)
        plane_c, plane_b, plane_w = (
            np.zeros((9, 9)), np.zeros((9, 9)), np.zeros((9, 9))
        )
        for y in range(1, 10):
            for x in range(1, 10):
                sq = miniosl.Square(x, y)
                eb = self.state.count_cover(miniosl.black, sq)
                ew = self.state.count_cover(miniosl.white, sq)
                if eb > 0 and ew > 0:
                    plane_c[y-1][x-1] = eb+ew
                elif eb > 0:
                    plane_b[y-1][x-1] = eb
                elif ew > 0:
                    plane_w[y-1][x-1] = ew
        self.draw_plane(plane_c, 'C3')
        self.draw_plane(plane_b, 'C4')
        self.draw_plane(plane_w, 'C5')

    def draw_plane(self, plane: np.ndarray, color: str,
                   stroke: int = 0) -> None:
        if plane.shape != (9, 9):
            raise ValueError(f'unexpected shape of plane {plane.shape}')
        maximum = np.max(plane)
        if maximum > 5:
            plane /= maximum
        weight = 1 if plane.max() > 2 or plane.max() == 0 else 4/plane.max()
        for y in range(1, 10):
            for x in range(1, 10):
                r = radius(weight*plane[y-1][x-1])
                self._decorate_square(x, y, r, color,
                                      markeredgewidth=0)

    def decorate_last_to(self, dst):
        x, y = dst.to_xy()
        if 'last_to' in self.arts:
            self.arts['last_to'].set_data([x], [y])
        else:
            sq, = self._decorate_square(x, y, 16, 'C0', marker='s',
                                        markeredgewidth=2, alpha=.3)
            self.arts['last_to'] = sq
        self.last_to = dst

    def draw_piece(self, x, y, piece):
        kanji = piece.ptype.to_ja1()
        if (x, y) in self.arts:
            update_letter(self.arts[(x, y)], x, y, kanji,
                          **properties[piece.color])
            return self.arts[(x, y)]
        if piece.color == miniosl.black:
            art = put_forward_char(self.ax, x, y, kanji)
        else:
            assert piece.color == miniosl.white
            art = put_reversed_char(self.ax, x, y, kanji)
        self.arts[(x, y)] = art
        return art

    def show_side_to_move(self, flipped: bool):
        player_to_move = "先手" if self.state.turn == miniosl.black else "後手"
        if 'turn' in self.arts:
            self.arts['turn'].set(text=f'手番 {player_to_move}')
        else:
            hoffset, voffset = hv_offset(self.ax)
            turn = put_forward_char(
                self.ax, 9.25+hoffset, 10+voffset, f'手番 {player_to_move}',
                size=10, alpha=.7, horizontalalignment='left',
                allow_outside=True
            )
            self.arts['turn'] = turn
        if 'last_move_ja' in self.arts:
            self.arts['last_move_ja'].set(text=f'({self.last_move_ja} まで)')
        else:
            art = add_last_move(self.ax, self.last_move_ja)
            self.arts['last_move_ja'] = art
        if 'move_number' not in self.arts:
            art = add_move_number(self.ax, '')  # might be None
            self.arts['move_number'] = art
        if self.move_number > 0:
            msg = f'{self.move_number}手目'
            if self.arts['move_number']:
                self.arts['move_number'].set(text=msg)
        if flipped:
            art = put_forward_char(self.ax, 2.25, 10.75, '先後反転', size=10,
                                   alpha=.7, horizontalalignment='left')
            self.arts['flipped'] = art
        return [self.arts[_]
                for _ in ['turn', 'last_move_ja', 'move_number', 'flipped']
                if _ in self.arts]

    def add_comment(self, msg):
        return add_move_number(self.ax, msg, hoffset=4)

    def hand_pieces_str(self, player: miniosl.Player) -> str:
        msg = hand_pieces_to_ja(self.state, player) + ' '*20
        if msg[19] != ' ':
            msg = msg[:19] + '他'
        return msg[:20]

    def place_pieces(self):
        changed = []
        for x in range(1, 10):
            for y in range(1, 10):
                if is_outside(self.ax, x, y):
                    continue
                piece = self.state.piece_at(miniosl.Square(x, y))
                if not piece.is_piece():
                    if (x, y) in self.arts:
                        self.arts[(x, y)].set(text='')
                        changed.append(self.arts[(x, y)])
                    continue
                art = self.draw_piece(x, y, piece)
                self.arts[(x, y)] = art
                changed.append(art)
        return changed

    def _place_hand_pieces(self, color: miniosl.Player):
        hand = self.hand_pieces_str(color)
        if color == miniosl.black:
            return place_black_hand_piece(self.ax, hand)
        else:
            return place_white_hand_piece(self.ax, hand)

    def _init_hand(self):
        for color in [miniosl.black, miniosl.white]:
            lst = self._place_hand_pieces(color)
            if color in self.arts:
                logging.warning('overwrite arts[color]')
            self.arts[color] = lst

    def update_hand(self, color, prev_hand_str=''):
        hand_str = self.hand_pieces_str(color)
        changed = []
        proc = place_black_hand_piece \
            if color == miniosl.black else place_white_hand_piece
        hand_arts = self.arts[color]
        for i, c in enumerate(hand_str):
            if i >= len(hand_arts):
                hand_arts += proc(self.ax, c, i)
                changed.append(hand_arts[-1])
            elif i >= len(prev_hand_str) or prev_hand_str[i] != c:
                if hand_arts[i]:
                    hand_arts[i].set(text=c)
                    changed.append(hand_arts[i])
        if len(prev_hand_str) > len(hand_str):
            for arts in hand_arts[len(hand_str):]:
                if arts:
                    arts.set(text='')
        changed += hand_arts[len(hand_str):]
        return changed

    def set_state(self, state,
                  last_move_ja: str = '',
                  last_to: miniosl.Square | None = None,
                  move_number: int = 0,
                  repeat_distance: int = 0,
                  repeat_count: int = 0, flipped: bool = False):
        # for internal use
        changed = self._clear_floating()
        self.state = miniosl.State(state)
        self.last_move_ja = last_move_ja
        self.last_to = last_to
        self.move_number = move_number
        changed += self.place_pieces()
        changed += self.update_hand(miniosl.black)
        changed += self.update_hand(miniosl.white)
        if last_to:
            self.decorate_last_to(last_to)
            changed.append(self.arts['last_to'])
        changed += self.show_side_to_move(flipped)
        if repeat_distance or repeat_count:
            logging.warning('repeat not supported yet')
        return changed

    def _clear_floating(self):
        changed = []
        if self.floating_piece and self.floating_piece[0]:
            piece = self.floating_piece[0]
            piece.remove()
            self.floating_piece = None
            changed.append(piece)
        return changed

    def _black_hand_src(self):
        return -1, min(6, self.ax.get_ylim()[0]-1)

    def _white_hand_src(self):
        return min(11, self.ax.get_xlim()[0]-1), 4

    def _make_floating_drop(self, move):
        kanji = move.ptype.to_ja1()
        if move.color == miniosl.black:
            proc = put_forward_char
            x, y = self._black_hand_src()
        else:
            proc = put_reversed_char
            x, y = self._white_hand_src()
        art = proc(self.ax, x, y, kanji)
        return [art, np.array([x, y])]

    def prepare_move(self, move: miniosl.Move, ratio: float):
        """draw an intermediate piece during move"""
        changed = []
        dst_xy = np.array(move.dst.to_xy())
        if not move.is_drop():
            # use the original piece on board
            src = move.src
            if src.to_xy() not in self.arts:
                return changed
            piece = self.arts[src.to_xy()]
            src_xy = np.array(src.to_xy())
        else:
            # make a floating piece
            if not self.floating_piece:
                self.floating_piece = self._make_floating_drop(move)
            piece, src_xy = self.floating_piece
        if not piece:
            return changed
        mratio = max(ratio-0.5, 0) * 2
        xy = (1-mratio) * src_xy + mratio * dst_xy
        if move.is_drop():
            self.floating_piece[1] = xy
        heps, veps = [
            properties[move.color][_] for _ in ['heps', 'veps']
        ]
        piece.set(position=(xy[0] + heps, xy[1] + veps))
        changed.append(piece)
        if move.is_capture() and self.arts.get(move.dst.to_xy(), None):
            cratio = min(ratio, 0.5)*2
            captured = self.arts[move.dst.to_xy()]
            captured_xy = np.array(move.dst.to_xy())
            cdst = (self._black_hand_src()
                    if move.color == miniosl.black else
                    self._white_hand_src())
            cxy = (1-cratio) * captured_xy + cratio * np.array(cdst)
            captured.set(
                position=(cxy[0] + heps, cxy[1] + veps),
                text=move.capture_ptype.unpromote().to_ja1(),
                rotation=(180 * (cratio ** .5) + max(move.color.sign, 0) * 180)
            )
            changed.append(captured)
        return [_ for _ in changed if _ is not None]

    def make_move(self, move: miniosl.Move | str):
        """update board and figure"""
        changed = self._clear_floating()
        if isinstance(move, str):
            move = self.state.to_move(move)
        if not self.state.is_legal(move):
            raise RuntimeError(f'{move.to_csa} not legal')
        self.last_move_ja = move.to_ja(self.state, self.last_to)
        dst = move.dst
        if move.is_capture():
            captured = self.arts[dst.to_xy()]
            captured.set(text='')
            changed.append(captured)
        moved = False
        if not move.is_drop() and self.arts.get(move.src.to_xy(), None):
            moved = True
            piece = self.arts[move.src.to_xy()]
            heps, veps = [
                properties[move.color][_] for _ in ['heps', 'veps']
            ]
            piece.set(position=(dst.x + heps, dst.y + veps))
            if move.is_promotion():
                piece.set(text=move.ptype.to_ja1())
            del self.arts[move.src.to_xy()]
            self.arts[dst.to_xy()] = piece
            changed.append(piece)

        color = move.color
        prev_hands = self.hand_pieces_str(color)
        self.state.make_move(move)

        if not moved:           # drop or src is outside board
            art = self.draw_piece(dst.x, dst.y, self.state.piece_at(dst))
            changed.append(art)
        if move.is_capture() or move.is_drop():
            changed += self.update_hand(color, prev_hands)
        self.move_number += 1
        self.decorate_last_to(dst)
        changed.append(self.arts['last_to'])
        changed += self.show_side_to_move(False)
        return [_ for _ in changed if _ is not None]


def state_to_img(state: miniosl.BaseState, *,
                 decorate: bool = False, plane: np.ndarray | None = None,
                 plane_color: str = 'orange',
                 last_move_ja: str = '',
                 last_to: miniosl.Square | None = None,
                 move_number: int = 0, repeat_distance: int = 0,
                 repeat_count: int = 0,
                 flip_if_white: bool = False,
                 id: int = 4081,
                 xlim=None, ylim=None,
                 ) -> matplotlib.figure.Figure:
    """make :py:class:`ShogiFig` object including matplotlib figure as `.fig`

    :param state: state,
    :param decorate: highlight king location and piece covers for each color,
    :param plane: 9x9 numpy array to make a mark on squares,
    :param last_move_ja: last move in japanese,
    :param last_to: the destination square of the last move,
    :param move_number: ply in a game record,
    :param repeat_distance: distance to the latest same position,
    :param repeat_count: number of the occurrence of this state,
    :param flip_if_white: `rotate180()` if white to move
    """
    flipped = False
    if flip_if_white and state.turn == miniosl.white:
        state = miniosl.State(state.rotate180())
        if last_to:
            last_to = last_to.rotate180()
        flipped = True
    if decorate and not isinstance(state, miniosl.State):
        logging.warning('promote BaseState to State for decoration')
        state = miniosl.State(state)

    fig = ShogiFig(state, last_move_ja=last_move_ja, move_number=move_number,
                   id=id, xlim=xlim, ylim=ylim)
    if decorate:
        fig.decorate_cover()

    if last_to:
        fig.decorate_last_to(last_to)

    if repeat_distance > 0:
        msg = f' ({repeat_distance}手前と同一局面 {repeat_count}回目)'
        fig.add_comment(msg)

    if plane is not None:
        fig.draw_plane(plane, plane_color, 'C6')

    fig.show_side_to_move(flipped)

    return fig


def show_channels(channels, nrows, ncols, flip=False, *, japanese=False):
    import matplotlib.pyplot as plt
    from mpl_toolkits.axes_grid1 import ImageGrid
    fig = plt.figure(figsize=(ncols*2.5, nrows*2))
    grid = ImageGrid(fig, 111, nrows_ncols=(nrows, ncols),
                     axes_pad=0.3, label_mode='all')
    dan = kanjirow[1:] if japanese else np.arange(1, 10)
    for i, ax in enumerate(grid):
        if flip:
            ax.set_xticks(np.arange(9), np.arange(9, 0, -1))
            ax.set_yticks(np.arange(9), reversed(dan))
        else:
            ax.set_xticks(np.arange(9), np.arange(1, 10))
            ax.set_yticks(np.arange(9), dan)
        ax.xaxis.tick_top()
        ax.yaxis.tick_right()
        ax.imshow(channels[i], cmap='Oranges', vmin=0, vmax=1,
                  interpolation='none')
        if flip:
            ax.invert_yaxis()
        else:
            ax.invert_xaxis()
        ax.tick_params(axis='both', length=0)
    return plt.show()


class ShogiAnimation:
    """"""
    def __init__(self,
                 record: miniosl.MiniRecord,
                 offset: int = 0, frame_per_move: int = 1,
                 xlim=None, ylim=None):
        self.record = record
        self.fig = miniosl.ShogiFig(record.initial_state, xlim=xlim, ylim=ylim)
        self.offset = offset
        self.last_n = None
        self.fpm = frame_per_move

    def _start(self):
        state = self.record.replay(self.offset)
        last_to = None
        if self.offset > 0:
            last_to = self.record.moves[self.offset-1].dst
        return self.fig.set_state(
            state,
            move_number=self.offset, last_to=last_to
        )

    def __call__(self, n):
        if n == 0:
            self.last_n = n
            return self._start()
        n, r = (n + self.fpm - 1) // self.fpm, (n % self.fpm) / self.fpm
        move = self.record.moves[self.offset + n-1]
        if r != 0:
            return self.fig.prepare_move(move, r)
        if n != self.last_n + 1:
            logging.warning(f'{self.last_n=} v.s. {n=}')
            self.last_n = n
            state = self.record.replay(self.offset + n)
            return self.fig.set_state(state, move_number=self.offset + n,
                                      last_to=move.dst)
        self.last_n = n
        return self.fig.make_move(move)

    def animate(self, moves, offset=None, **kwargs):
        import matplotlib.animation
        if plt.rcParams["animation.html"] == 'none':
            plt.rc('animation', html='jshtml')
        if offset is not None:
            self.offset = offset
        moves = min(moves, self.record.move_size() - self.offset + 1)
        frames = moves * self.fpm + 1
        self.anim = matplotlib.animation.FuncAnimation(
            self.fig.fig, self, init_func=self._start, frames=frames,
            **kwargs, blit=True,
        )
        return self.anim
