import miniosl
import miniosl.drawing
import ipywidgets
import matplotlib.pyplot as plt
import IPython

ctl_layout = {'width': '4em', 'height': '5ex'}
square_layout = {'width': '2em', 'height': '4ex', 'padding': '0'}
centered_layout = {
    'display': 'flex', 'justify_content': 'center',
}
right_aligned_layout = {
    'display': 'flex', 'justify_content': 'flex-end',
}

default_fpm = 16
default_interval = 50


class Controls:
    def __init__(self):
        # https://en.wikipedia.org/wiki/Geometric_Shapes_(Unicode_block)
        Button = ipywidgets.Button
        self.left_btn = Button(description='◂', layout=ctl_layout)
        self.right_btn = Button(description='▶', layout=ctl_layout)
        self.dleft_btn = Button(description='<<', layout=ctl_layout)
        self.dright_btn = Button(description='>>', layout=ctl_layout)
        self.uleft_btn = Button(description='|◂', layout=ctl_layout)
        self.uright_btn = Button(description='▶|', layout=ctl_layout)

    def disable(self, backward, forward):
        self.left_btn.disabled = backward
        self.dleft_btn.disabled = backward
        self.uleft_btn.disabled = backward
        self.right_btn.disabled = forward
        self.dright_btn.disabled = forward
        self.uright_btn.disabled = forward

    def on_click(self, callback):
        self.left_btn.on_click(callback)
        self.right_btn.on_click(callback)
        self.dleft_btn.on_click(callback)
        self.dright_btn.on_click(callback)
        self.uleft_btn.on_click(callback)
        self.uright_btn.on_click(callback)

    def widget_box(self):
        return ipywidgets.HBox([
            self.uleft_btn, self.dleft_btn, self.left_btn,
            self.right_btn, self.dright_btn, self.uright_btn,
        ])


class BoardView:
    def __init__(self, border='1px solid black'):
        self.out = ipywidgets.Output(layout={
            'width': '3.5in',
            'min_width': '3.5in',
            'height': '3.5in',
            'border': border,
            'margin': '0ex 1em 0ex 1em'
        })
        # initialized later
        self.anim = None

    def update(self, img):
        with self.out:
            self.out.clear_output(wait=True)
            display(img)

    def clear_animation(self):
        self.anim = None

    def make_animation(self, *args, frame_per_move=default_fpm, **kwargs):
        if not self.anim:
            with self.out:
                self.anim = miniosl.ShogiAnimation(
                    frame_per_move=frame_per_move,
                    *args, **kwargs
                )

    def animate(self, *args, interval=default_interval, **kwargs):
        board = IPython.display.Video(
            self.anim.animate(
                *args, interval=interval, **kwargs
            ).to_html5_video(),
            embed=True, html_attributes='autoplay'
        )
        self.update(board)

    def widget_box(self):
        return self.out


class RecordView:
    def __init__(self, ui: miniosl.UI):
        ja_moves = ui._record.to_ja()
        record_len = len(ui._record)
        self.move_btns = [
            ipywidgets.Button(
                description=f'{ja_moves[i]:8s}',
                layout={'height': '3.5ex', 'display': 'flex',
                        'justify_content': 'flex-start'},
                style={'font_size': '75%'},
            )
            for i in range(record_len)
        ]
        self.show_cover_btn = ipywidgets.Button(
            description='利き表示・解除', layout={'width': '11em'},
            button_style='info',
        )
        self.cover_showing = False

        # ipywidgets.Checkbox(value=False, description='利き表示')
        self.ctrls = Controls()
        self.board = BoardView(border='0px')
        MN = 10
        anim_moves = 1

        def callback(widget):
            if widget is self.ctrls.right_btn:
                ui.go(1)
            elif widget is self.ctrls.left_btn:
                ui.go(-1)
            elif widget is self.ctrls.dright_btn:
                step = min(10, record_len - ui.cur)
                anim_moves = step
                ui.go(step)
            elif widget is self.ctrls.dleft_btn:
                step = min(10, ui.cur)
                ui.go(-step)
            elif widget is self.ctrls.uright_btn:
                ui.last()
            elif widget is self.ctrls.uleft_btn:
                ui.first()
            elif widget in self.move_btns:
                ui.replay(self.move_btns.index(widget) + 1)
            self.ctrls.disable(ui.cur == 0, ui.cur >= record_len)
            self.move_tab.selected_index = min(
                ui.cur // (MN*2),
                len(self.move_tab.children) - 1
            )
            show_cover_requested = widget is self.show_cover_btn
            if ui.cur == 0 or show_cover_requested:
                if show_cover_requested:
                    self.cover_showing = not self.cover_showing
                board = ui.to_img(decorate=self.cover_showing)
                self.board.update(board)
                self.board.clear_animation()
            else:
                self.board.make_animation(ui._record)
                self.board.animate(anim_moves, offset=ui.cur-anim_moves)

        self.show_cover_btn.on_click(callback)
        self.ctrls.on_click(callback)
        for btn in self.move_btns:
            btn.on_click(callback)
        self.move_tab = ipywidgets.Tab()
        move_num_layout = {
            'display': 'flex',
            'justify_content': 'flex-end',
            'height': '3.5ex',
        }
        self.move_tab.children = [
            ipywidgets.HBox([
                ipywidgets.VBox([
                    ipywidgets.Label(str(_+1), layout=move_num_layout)
                    for _ in range(d*MN*2, min((d+1)*MN*2, record_len), 2)
                ]),
                ipywidgets.VBox(self.move_btns[d*MN*2:(d+1)*MN*2:2]),
                ipywidgets.VBox([
                    ipywidgets.Label(str(_+1), layout=move_num_layout)
                    for _ in range(d*MN*2+1, min((d+1)*MN*2, record_len), 2)
                ]),
                ipywidgets.VBox(self.move_btns[d*MN*2+1:(d+1)*MN*2:2]),
            ])
            for d in range((record_len + MN*2-1) // (MN*2))
        ]
        self.move_tab.titles = [
            f'{d*MN*2+1}-'      # {min((d+1)*MN, record_len)}
            for d in range((record_len + MN*2-1) // (MN*2))
        ]
        self.widget = ipywidgets.VBox([
            ipywidgets.Label('棋譜表示'),
            ipywidgets.HBox([
                self.board.widget_box(),
                ipywidgets.HBox(
                    [self.move_tab, ipywidgets.Label(' ')],
                    layout={'overflow': 'scroll hidden', 'display': 'flex'}
                )
            ]),
            ipywidgets.HBox([self.ctrls.widget_box(), self.show_cover_btn]),
        ])
        callback(None)

        plt.ioff()
        plt.close()


sq_bg_color = '#f8d38b'
sq_dst_color = '#d3f88b'
sq_dst2_color = '#53b85b'
move_src_unselected = '(未選択)'


class SquareControl:
    """
    states:
    (1) fresh --- just after set_state
        moves, moves_ja are initialized
    (2) src selected --- after choosing a piece on board or stand
        dst_candidate is set, move_select is filtered
    (3) dst selected --- after choosing a destination square at (2)
    """
    def __init__(self, parent, xlim=9, ylim=9):
        self.parent = parent
        self.xlim = xlim
        self.ylim = ylim
        self.black_stand, self.black_stand_box = self.make_stand('先手')
        self.white_stand, self.white_stand_box = self.make_stand('後手')
        self.squares = [[
            ipywidgets.Button(
                description=str(xlim-col)+str(1+row),
                layout=square_layout,
                style=dict(font_size='90%', button_color=sq_bg_color)
            ) for col in range(xlim)
        ] for row in range(ylim)]
        self.rows = [
            ipywidgets.HBox(
                self.squares[row]
                + [ipywidgets.Label(
                    miniosl.drawing.kanjirow[row+1],
                    layout={'width': '1em'}
                )])
            for row in range(ylim)
        ]
        self.move_select = ipywidgets.Dropdown(
            options=[''], description='指し手候補',
            layout={'width': '16em'},
        )
        self.selected_sq = None
        self.dst_candidate = []
        self.stand_ptype = []
        self.move_lbl = ipywidgets.Label('動かす駒')
        self.move_src = ipywidgets.Label(move_src_unselected)
        self.move_now = ipywidgets.Button(
            description='指す', layout={'width': '6em'},
            button_style='success',
        )
        self.turn_label = ipywidgets.Label('')
        board_main = ipywidgets.HBox([
            self.white_stand_box,
            ipywidgets.VBox(
                [ipywidgets.HBox([ipywidgets.Label(
                    f' {xlim-col}',
                    layout=(centered_layout | {'width': '1.95em'})
                ) for col in range(xlim)])]
                + self.rows,
                layout={'margin': '0 0em 0 1em'}),
            self.black_stand_box,
        ], layout={'margin': '0 0em 0 3em'})

        self.board = ipywidgets.VBox([
            ipywidgets.HBox([ipywidgets.Label('指し手選択'), self.turn_label],
                            layout=centered_layout),
            board_main,
            ipywidgets.HBox([self.move_lbl, self.move_src, ],
                            layout=centered_layout),
            ipywidgets.HBox([self.move_select, self.move_now],
                            layout=right_aligned_layout)
        ])

        def move_now(widget):
            if not self.move_select.value:
                return
            self.selected_move = self.ja_to_move(self.move_select.value)
            self.parent.make_move(self.selected_move)

        self.move_now.on_click(move_now)
        self.move_select.observe(lambda chg: self.move_selected(),
                                 names='value')

        def callback_xy(widget):
            for y in range(self.ylim):
                if widget not in self.squares[y]:
                    continue
                x = self.squares[y].index(widget)
                self.select_xy(self.xlim-x, y+1)
                return

        for y in range(self.ylim):
            for x in range(self.xlim):
                self.squares[y][x].on_click(callback_xy)

        def callback_stand(widget):
            stand = (
                self.black_stand
                if self.state.turn == miniosl.black
                else self.white_stand
            )
            idx = stand.index(widget)
            self.select_ptype(self.stand_ptype[idx])

        for i in range(7):
            self.black_stand[i].on_click(callback_stand)
            self.white_stand[i].on_click(callback_stand)

    def move_selected(self):
        self.move_now.disabled = not bool(self.move_select.value)
        if self.move_select.value:
            selected_move = self.ja_to_move(self.move_select.value)
            if selected_move.is_drop():
                self.first_selection(selected_move.ptype,
                                     from_move_select=True)
            else:
                sq = miniosl.Square(selected_move.src.x, selected_move.src.y)
                piece = self.state.piece_at(sq)
                self.first_selection(piece, from_move_select=True)
        else:
            self.reset_selection()

    def make_stand(self, name):
        btns = [ipywidgets.Button(
            description=_,
            layout=(square_layout | {'margin': '1ex 0 0ex 0'}),
            style=dict(font_size='90%', button_color=sq_bg_color),
            disabled=True,
        ) for _ in "       "]
        layout = {'height': '3ex', 'margin': '0', 'padding': '0'}
        lbls = [
            ipywidgets.Label('持駒', layout=layout),
            ipywidgets.Label(name, layout=layout)
        ]
        return btns, ipywidgets.VBox(lbls+btns, layout=centered_layout)

    def reset_dst(self):
        for x, y in self.dst_candidate:
            btn = self.squares[y-1][self.xlim-x]
            btn.style.button_color = sq_bg_color
            btn.disabled = True
        self.dst_candidate = []

    def select_ptype(self, ptype):
        if self.selected_sq == ptype:
            self.reset_selection()
            return
        self.first_selection(ptype)

    def reset_selection(self):
        self.move_src.value = move_src_unselected
        self.move_select.options = self.moves_ja
        self.move_select.value = None
        self.reset_dst()
        self.selected_sq = None
        self.move_now.description = '指す'

    def select_xy(self, x, y):
        if self.selected_sq == (x, y):
            # this is a second clicked to unselect
            self.reset_selection()
            return
        sq = miniosl.Square(x, y)
        piece = self.state.piece_at(sq)
        if not piece.is_piece() or piece.color != self.state.turn:
            # destination selected
            self.filter_moves(dst=sq)
            self.show_current_dst()
            return
        # first click choose src
        self.first_selection(piece)

    def first_selection(self, src, from_move_select=False):
        if type(src) is miniosl.Piece:
            self.selected_sq = src.square.to_xy()
            self.move_src.value = (
                src.square.to_ja() + src.ptype.to_ja()
            )
            self.move_now.description = '指す'
        else:
            assert type(src) is miniosl.Ptype
            self.selected_sq = src
            self.move_src.value = (
                src.to_ja() + ' (持駒)'
            )
            self.move_now.description = '打つ'
        if not from_move_select:
            self.filter_moves()
        self.reset_dst()
        self.dst_candidate = [
            _.dst.to_xy()
            for _ in self.moves
            if self.move_match(_, self.selected_sq)
        ]
        self.activate_dst()
        self.show_current_dst()

    def show_current_dst(self):
        if self.move_select.value:
            self.activate_dst()
            selected_move = self.ja_to_move(self.move_select.value)
            dst = selected_move.dst
            btn = self.squares[dst.y-1][self.xlim-dst.x]
            btn.style.button_color = sq_dst2_color

    def move_match(self, move, src, dst=None):
        if type(src) is miniosl.Ptype:
            if (not move.is_drop()) or move.ptype != src:
                return False
        else:
            if move.src.to_xy() != src:
                return False
        return (not dst) or move.dst == dst

    def filter_moves(self, dst=None):
        self.move_select.options = [
            _ for _ in self.moves_ja
            if self.move_match(self.ja_to_move(_),
                               self.selected_sq, dst)
        ]
        self.move_select.value = self.move_select.options[0]

    def activate_dst(self):
        for x, y in self.dst_candidate:
            btn = self.squares[y-1][self.xlim-x]
            btn.style.button_color = sq_dst_color
            btn.disabled = False

    def ja_to_move(self, ja):
        return self.moves[self.moves_ja.index(ja)]

    def widget_box(self):
        return self.board

    def piece_str(piece):
        if not piece.is_piece():
            return ' '
        return ((' ' if piece.color == miniosl.black else 'v')
                + piece.ptype.to_ja1())

    def set_state(self, state, ai_moves=None):
        self.reset_dst()
        self.state = state
        if ai_moves is None:
            self.moves = list(state.genmove_full())
            self.moves.sort(key=lambda e: e.dst.to_xy())
        else:
            self.moves = [_ for _ in ai_moves]
        self.moves_ja = [_.to_ja(state) for _ in self.moves]
        self.move_select.options = self.moves_ja
        self.move_select.value = (
            self.moves_ja[0] if len(self.moves) == 1 else None
        )
        self.legal_move_src = set(_.src.to_xy() for _ in self.moves)
        for y in range(self.ylim):
            for x in range(self.xlim):
                sq = miniosl.Square(x+1, y+1)
                piece = state.piece_at(sq)
                btn = self.squares[y][self.xlim-1-x]
                btn.description = SquareControl.piece_str(piece)
                btn.disabled = sq.to_xy() not in self.legal_move_src
        self.update_stand(miniosl.black, self.black_stand)
        self.update_stand(miniosl.white, self.white_stand)
        self.selected_sq = None
        self.move_src.value = move_src_unselected
        self.turn_label.value = '先手番' if state.turn == miniosl.black else '後手番'
        if self.state.in_checkmate():
            self.turn_label.value += ' (詰み)'
        elif self.state.in_check():
            self.turn_label.value += ' (王手)'

    def update_stand(self, player, stand_btns):
        stand_ptype = []
        for ptype in miniosl.piece_stand_order:
            if self.state.count_hand(player, ptype) > 0:
                stand_ptype.append(ptype)
        for i in range(7):
            if i < len(stand_ptype):
                stand_btns[i].disabled = (player != self.state.turn)
                stand_btns[i].description = stand_ptype[i].to_ja1()
            else:
                stand_btns[i].disabled = True
                stand_btns[i].description = ' '
        if self.state.turn == player:
            self.stand_ptype = stand_ptype


class PlayWindow:
    def __init__(self, ui: miniosl.UI):
        self.with_ai_assist = False
        ui.load_eval()
        has_ai = ui.model
        self.board = BoardView()
        self.sq_ctrl = SquareControl(self)
        self.show_cover_btn = ipywidgets.Button(
            description='利き表示', layout={'width': '8em'},
            button_style='info',
        )
        self.ai_assist_btn = ipywidgets.Button(
            description='AI支援', layout={'width': '8em'},
            button_style='info',
        )
        self.auto_reply_btn = ipywidgets.Button(
            description='後手をAIが進める', layout={'width': '12em'},
            button_style='warning',
        )
        self.cover_showing = False
        self.ai_showing = False
        self.show_cover_btn.on_click(lambda w: self.button_pressed(w))
        self.ai_assist_btn.on_click(lambda w: self.button_pressed(w))
        self.auto_reply_btn.on_click(lambda w: self.button_pressed(w))
        self.reset_btn = ipywidgets.Button(
            description='初めに戻す', layout={'width': '8em'},
            button_style='danger',
        )
        self.reset_btn.on_click(lambda w: self.button_pressed(w))
        self.reset_btn.disabled = True

        self.widget = ipywidgets.HBox([
            ipywidgets.VBox([
                ipywidgets.Label('対局盤面'),
                self.board.widget_box(),
                ipywidgets.HBox([
                    self.show_cover_btn, self.ai_assist_btn,
                ]),
                self.reset_btn,
            ]),
            ipywidgets.VBox([
                self.sq_ctrl.widget_box(),
                ipywidgets.HBox(
                    [self.auto_reply_btn], layout=right_aligned_layout,
                ),
            ], layout=centered_layout)
        ])
        self.ui = ui
        self.board.update(ui.to_img())
        self.sq_ctrl.set_state(ui._state)
        plt.ioff()
        plt.close()
        self.auto_reply_btn.disabled = (
            self.ui.in_checkmate() or self.ui.turn() == miniosl.black
        )

    def button_pressed(self, widget):
        if widget is self.auto_reply_btn:
            moves = self.ai_moves()
            self.make_move(moves[0][1])
            return
        if widget is self.show_cover_btn:
            self.cover_showing = not self.cover_showing
            self.ai_showing = False
        elif widget is self.ai_assist_btn:
            self.cover_showing = False
            self.ai_showing = not self.ai_showing
        elif widget is self.reset_btn:
            self.ui.first()
            self.cover_showing = False
            self.ai_showing = False
        self.make_move(None)

    def ai_moves(self):
        if not self.ui.model:
            return None
        gumbel_cscale = 8
        values = self.ui.gumbel_one(width=8, cscale=gumbel_cscale)
        ret = [(_[0], _[1]) for _ in values]
        top_moves = set(_[1] for _ in ret)
        mp = self.ui._infer_result['mp']
        ret += [(_[0], _[1]) for _ in mp if _[1] not in top_moves]
        return ret

    def make_move(self, move):
        if move:
            self.cover_showing = False
            self.ai_showing = False
            self.ui.make_move(move)
            self.board.clear_animation()
            self.board.make_animation(self.ui._record)
            self.board.animate(1, offset=self.ui.cur-1)
        else:
            if self.ai_showing:
                import numpy as np
                move_width = 4
                mp = self.ai_moves()
                plane = np.zeros((9, 9))
                for i in range(min(move_width, len(mp))):
                    dst = mp[i][1].dst
                    plane[dst.y-1, dst.x-1] += mp[i][0]
                img = self.ui.to_img(plane=plane)
            else:
                img = self.ui.to_img(decorate=self.cover_showing)
            self.board.update(img)
        if move or self.ai_showing or self.ui.cur == 0:
            self.sq_ctrl.set_state(
                self.ui._state,
                [_[1] for _ in mp] if self.ai_showing else None
            )
        self.auto_reply_btn.disabled = (
            self.ui.in_checkmate() or self.ui.turn() == miniosl.black
        )
        self.reset_btn.disabled = not self.ui.in_checkmate()


class PuzzleWindow:
    def __init__(self, puzzle: miniosl.Puzzle):
        puzzle.load_eval()
        self.board = BoardView()
        self.sq_ctrl = SquareControl(self, puzzle.xlim, puzzle.ylim)
        self.show_cover_btn = ipywidgets.Button(
            description='利き表示', layout={'width': '8em'},
            button_style='info',
        )
        self.cover_showing = False
        self.auto_reply_btn = ipywidgets.Button(
            description='後手をAIが進める', layout={'width': '12em'},
            button_style='warning',
        )
        self.comment = ipywidgets.Output(layout={
            'flex_flow': 'row wrap',
            'width': '3.3in',
            'margin': '0em'
        })
        self.reset_btn = ipywidgets.Button(
            description='初めに戻す', layout={'width': '8em'},
            button_style='danger',
        )
        self.show_cover_btn.on_click(lambda w: self.button_pressed(w))
        self.auto_reply_btn.on_click(lambda w: self.button_pressed(w))
        self.reset_btn.on_click(lambda w: self.button_pressed(w))
        self.widget = ipywidgets.HBox([
            ipywidgets.VBox([
                ipywidgets.Label('盤面'),
                self.board.widget_box(),
                self.show_cover_btn,
                self.comment,
                self.reset_btn,
            ]),
            ipywidgets.VBox([
                self.sq_ctrl.widget_box(),
                ipywidgets.HBox(
                    [self.auto_reply_btn], layout=right_aligned_layout,
                ),
            ]),
        ])
        self.puzzle = puzzle
        self.reset()
        plt.ioff()
        plt.close()
        self.auto_reply_btn.disabled = True

    def button_pressed(self, widget):
        if widget is self.reset_btn:
            self.reset()
        elif widget is self.show_cover_btn:
            self.cover_showing = not self.cover_showing
            self.make_move(None)
        elif widget is self.auto_reply_btn:
            gumbel_cscale = 8
            values = self.puzzle.gumbel_one(width=8, cscale=gumbel_cscale)
            self.make_move(values[0][1])

    def reset(self):
        self.puzzle.reset()
        self.board.update(self.puzzle.to_img())
        self.sq_ctrl.set_state(self.puzzle._state, self.puzzle.move_cache)
        with self.comment:
            self.comment.clear_output(wait=True)
            print(self.puzzle.comment())
        self.reset_btn.disabled = True

    def make_move(self, move):
        if move:
            self.reset_btn.disabled = False
            self.puzzle.make_move(move)
            self.board.clear_animation()
            self.board.make_animation(
                self.puzzle._record,
                xlim=self.puzzle.xlim, ylim=self.puzzle.ylim
            )
            self.board.animate(1, offset=self.puzzle.cur-1)
            with self.comment:
                self.comment.clear_output(wait=True)
                print(self.puzzle.comment())
        else:
            img = self.puzzle.to_img(decorate=self.cover_showing)
            self.board.update(img)
        if move:
            self.sq_ctrl.set_state(self.puzzle._state, self.puzzle.move_cache)
        self.auto_reply_btn.disabled = (self.puzzle.turn() == miniosl.black)


def game_play(ui: miniosl.UI):
    ui.playing_window = PlayWindow(ui)
    return ui.playing_window.widget


def record_view(ui: miniosl.UI):
    ui.record_view = RecordView(ui)
    return ui.record_view.widget


def slider_ui(ui: miniosl.UI):
    slider = ipywidgets.IntSlider(
        min=0,
        max=len(ui._record),
        value=ui.cur,
        description='Move number',
    )
    if not ui.fig:
        ui.to_img()
    fig = ui.fig

    def update(n):
        ui.replay(n)
        cfg = {}
        ui._add_drawing_properties(cfg)
        fig.set_state(ui._state, **cfg)
        return fig.fig
    return ipywidgets.interact(update, n=slider)
