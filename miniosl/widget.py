import miniosl
import miniosl.drawing
import ipywidgets
import matplotlib.pyplot as plt

ctl_layout = {'width': '4em', 'height': '5ex'}
square_layout = {'width': '2em', 'height': '4ex', 'padding': '0'}
stand_layout = {
    'display': 'flex', 'justify_content': 'center',
}


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
    def __init__(self):
        self.out = ipywidgets.Output(layout={
            'width': '3.5in',
            'min_width': '3.5in',
            'border': '1px solid black',
            'margin': '0em'
        })

    def update(self, img):
        with self.out:
            self.out.clear_output(wait=True)
            display(img)

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
        self.show_cover = ipywidgets.Checkbox(value=False, description='利き表示')
        self.ctrls = Controls()
        self.board = BoardView()
        MN = 10

        def callback(widget):
            if widget is self.ctrls.right_btn:
                ui.go(1)
            elif widget is self.ctrls.left_btn:
                ui.go(-1)
            elif widget is self.ctrls.dright_btn:
                step = min(10, record_len - ui.cur)
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
            self.board.update(ui.to_img(decorate=self.show_cover.value))

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
            ipywidgets.HBox([self.ctrls.widget_box(), self.show_cover]),
        ])
        callback(None)

        plt.ioff()
        plt.close()
        self.show_cover.observe(callback, names='value')


sq_bg_color = '#f8d38b'
sq_dst_color = '#d3f88b'
move_src_unselected = '--'


class SquareControl:
    """
    states:
    (1) fresh --- just after set_state
        moves, moves_ja are initialized
    (2) src selected --- after choosing a piece on board or stand
        dst_candidate is set, move_select is filtered
    (3) dst selected --- after choosing a destination square at (2)
    """
    def __init__(self, parent):
        self.parent = parent
        self.black_stand, self.black_stand_box = self.make_stand('先手')
        self.white_stand, self.white_stand_box = self.make_stand('後手')
        self.squares = [[
            ipywidgets.Button(
                description=str(9-col)+str(1+row),
                layout=square_layout,
                style=dict(font_size='90%', button_color=sq_bg_color)
            ) for col in range(9)
        ] for row in range(9)]
        self.rows = [
            ipywidgets.HBox(
                self.squares[row]
                + [ipywidgets.Label(
                    miniosl.drawing.kanjirow[row+1],
                    layout={'width': '1em'}
                )])
            for row in range(9)
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
            description='指す', layout={'width': '6em'}
        )
        self.turn_label = ipywidgets.Label('')
        self.board = ipywidgets.HBox([
            self.white_stand_box,
            ipywidgets.VBox(
                [ipywidgets.HBox([ipywidgets.Label('指し手選択'), self.turn_label])]
                + [ipywidgets.HBox([ipywidgets.Label(
                    f' {9-col}', layout={
                        'display': 'flex', 'justify_content': 'center',
                        'width': '1.95em'
                    }) for col in range(9)])]
                + self.rows
                + [ipywidgets.HBox([self.move_lbl, self.move_src, ])]
                + [ipywidgets.HBox([self.move_select, self.move_now])],
                layout={'margin': '0 0em 0 1em'}),
            self.black_stand_box,
        ], layout={'margin': '0 0em 0 3em'})

        def move_now(widget):
            if not self.move_select.value:
                return
            self.selected_move = self.ja_to_move(self.move_select.value)
            self.parent.make_move(self.selected_move)

        self.move_now.on_click(move_now)

        def callback_xy(widget):
            for y in range(9):
                if widget not in self.squares[y]:
                    continue
                x = self.squares[y].index(widget)
                self.select_xy(9-x, y+1)
                return

        for y in range(9):
            for x in range(9):
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
        return btns, ipywidgets.VBox(lbls+btns, layout={
            'display': 'flex', 'justify_content': 'center',
        })

    def reset_dst(self):
        for x, y in self.dst_candidate:
            btn = self.squares[y-1][9-x]
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
            return
        # first click choose src
        self.first_selection(piece)

    def first_selection(self, src):
        if type(src) is miniosl.Piece:
            self.selected_sq = src.square.to_xy()
            self.move_src.value = (
                src.square.to_ja() + src.ptype.to_ja()
            )
        else:
            assert type(src) is miniosl.Ptype
            self.selected_sq = src
            self.move_src.value = (
                src.to_ja() + ' (持駒)'
            )
        self.filter_moves()
        self.reset_dst()
        self.dst_candidate = [
            _.dst.to_xy()
            for _ in self.moves
            if self.move_match(_, self.selected_sq)
        ]
        self.activate_dst()

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
            btn = self.squares[y-1][9-x]
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
        if not ai_moves:
            self.moves = list(state.genmove_full())
            self.moves.sort(key=lambda e: e.dst.to_xy())
        else:
            self.moves = [_[1] for _ in ai_moves]
        self.moves_ja = [_.to_ja(state) for _ in self.moves]
        self.move_select.options = self.moves_ja
        self.move_select.value = None
        self.legal_move_src = set(_.src.to_xy() for _ in self.moves)
        for y in range(9):
            for x in range(9):
                sq = miniosl.Square(x+1, y+1)
                piece = state.piece_at(sq)
                btn = self.squares[y][8-x]
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
        view_opt = ['標準', '利き表示', 'AI推薦']
        self.assist = ipywidgets.RadioButtons(
            options=view_opt if has_ai else view_opt[:2]
        )
        self.widget = ipywidgets.HBox([
            ipywidgets.VBox([
                ipywidgets.Label('対局盤面'),
                self.board.widget_box(),
                self.assist,
            ]),
            self.sq_ctrl.widget_box(),
        ])
        self.ui = ui
        self.board.update(ui.to_img())
        self.sq_ctrl.set_state(ui._state)
        plt.ioff()
        plt.close()
        self.assist.observe(lambda chg: self.make_move(None),
                            names='value')

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
            self.ui.make_move(move)
        with_ai_assist = self.assist.value == self.assist.options[2]
        if with_ai_assist:
            import numpy as np
            move_width = 4
            mp = self.ai_moves()
            plane = np.zeros((9, 9))
            for i in range(min(move_width, len(mp))):
                dst = mp[i][1].dst
                plane[dst.y-1, dst.x-1] += mp[i][0]
            img = self.ui.to_img(plane=plane)
        else:
            decorate = (self.assist.value == self.assist.options[1])
            img = self.ui.to_img(decorate=decorate)
        self.board.update(img)
        if move or (with_ai_assist != self.with_ai_assist):
            self.sq_ctrl.set_state(
                self.ui._state,
                mp if with_ai_assist else None
            )
        self.with_ai_assist = with_ai_assist


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
