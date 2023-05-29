from __future__ import annotations
import minioslcc
import drawsvg
import random

def is_in_notebook() -> bool:
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


csadict = {
    'FU': '歩', 'KY': '香', 'KE': '桂', 'GI': '銀', 'KI': '金', 'KA': '角', 'HI': '飛', 'OU': '王',
    'TO': 'と', 'NY': '杏', 'NK': '圭', 'NG': '全', 'UM': '馬', 'RY': '龍',
}
kanjirow = [None, '一', '二', '三', '四', '五', '六', '七', '八', '九']
kanjicol = [None, '1', '2', '3', '4', '5', '6', '7', '8', '9']

def make_hand_rep(state: State, player: minioslcc.Player) -> str:
    ret = ''
    for ptype in minioslcc.piece_stand_order():
        cnt = state.count_hand(player, ptype)
        if cnt == 0:
            continue
        ret += csadict[minioslcc.to_csa(ptype)] * cnt
    if ret == '':
        ret = 'なし'
    return ret

def csaboard_to_svg(state: State, id: int, *, decorate=False) -> drawsvg.drawing.Drawing:
    import drawsvg as dw
    import cairosvg
    scale = 20
    def gx(x):
        return (11-x)*scale+scale/2
    def gy(y):
        return (y+1)*scale
    def radius(cnt):
        return scale/4 * (cnt ** 1/4)
    d = dw.Drawing(290, 270, id_prefix=str(id))
    d.append_css(".piece { font-family: Noto Serif CJK JP; }")
    char_property = {'class': "piece", 'fill': 'black' }
    sub_property = {'class': "piece", 'fill': 'gray' }
    b_property = {'fill': 'orange', 'fill_opacity': 0.2, 'stroke_width': 0 }
    w_property = {'fill': 'blue', 'fill_opacity': 0.2, 'stroke_width': 0 }
    c_property =  {'fill': 'purple', 'fill_opacity': 0.2, 'stroke_width': 0 }
    in_check_color = {'fill': 'red', 'fill_opacity': 0.5, 'stroke_width': 0 }
    last_move_property =  {'fill': 'yellow', 'fill_opacity': 0.2, 'stroke_width': 0 }
    if decorate:
        kb = state.king_square(minioslcc.black)
        kw = state.king_square(minioslcc.white)
        if kb.is_onboard():
            prop = in_check_color if state.in_check() and state.turn() == minioslcc.black else b_property
            x,y = kb.to_xy()
            prop['fill_opacity'] = 0.1
            d.append(dw.Rectangle(gx(x),gy(y-1),scale,scale,**prop))
        if kw.is_onboard():
            prop = in_check_color if state.in_check() and state.turn() == minioslcc.white else w_property
            x,y = kw.to_xy()
            prop['fill_opacity'] = 0.1
            d.append(dw.Rectangle(gx(x),gy(y-1),scale,scale,**prop))
            
        for x in range(1,10):
            for y in range(1,10):
                eb = state.count_cover(minioslcc.black, minioslcc.Square(x, y))
                ew = state.count_cover(minioslcc.white, minioslcc.Square(x, y))
                if eb > 0 and ew > 0:
                    d.append(dw.Circle(gx(x-.5),gy(y-.5),radius(eb+ew), **c_property))
                elif eb > 0:
                    d.append(dw.Circle(gx(x-.5),gy(y-.5),radius(eb),**b_property))
                elif ew > 0:
                    d.append(dw.Circle(gx(x-.5),gy(y-.5),radius(ew),**w_property))
        
    if last_to := state.last_to():
        x, y = last_to.to_xy()
        d.append(dw.Rectangle(gx(x),gy(y-1),scale,scale,**last_move_property))
    for x in range(1,10):
        for y in range(1,10):
            piece = state.piece_at(minioslcc.Square(x, y))
            if not piece.is_piece():
                continue
            kanji = csadict[minioslcc.to_csa(piece.ptype())]
            if piece.owner() == minioslcc.black:
                d.append(dw.Text(kanji, font_size=scale*.875, x=gx(x)+scale*.05, y=gy(y)-scale*.125,
                                      **char_property))
            else:
                assert piece.owner() == minioslcc.white
                d.append(dw.Text(kanji, font_size=scale*.875, x=gx(x)+scale*.9, y=gy(y)-scale*.8,
                                      rotate='180', **char_property))
    for i in range(1,10):
        d.append(dw.Text(kanjicol[10-i], font_size=scale*.75, x=gx(i)+scale*.25, y=gy(0)-scale*0.25,
                              **sub_property))
        d.append(dw.Text(kanjirow[i], font_size=scale*.625, x=gx(0)+scale/4, y=gy(i)-scale*0.25,
                              **sub_property))
        if i < 9:
            d.append(dw.Line(gx(0), gy(i), gx(9), gy(i), stroke='gray'))
            d.append(dw.Line(gx(i), gy(0), gx(i), gy(9), stroke='gray'))
    d.append(dw.Line(gx(0), gy(0), gx(9), gy(0), stroke='black'))
    d.append(dw.Line(gx(0), gy(9), gx(9), gy(9), stroke='black'))
    d.append(dw.Line(gx(0), gy(0), gx(0), gy(9), stroke='black'))
    d.append(dw.Line(gx(9), gy(0), gx(9), gy(9), stroke='black'))
    player_to_move = "先手" if state.turn() == minioslcc.black else "後手"
    d.append(dw.Text('手番 '+player_to_move, font_size=scale*.75, x=gx(9)+scale*.05, y=gy(10),
                     **char_property))
    if state.last_move_ja:
        d.append(dw.Text('('+state.last_move_ja + ' まで)', font_size=scale*.7, x=gx(5)+scale*.05, y=gy(10),
                         **char_property))
        
    hand_b = '先手持駒 ' + make_hand_rep(state, minioslcc.black)
    hand_w = '後手持駒 ' + make_hand_rep(state, minioslcc.white)
    for i, c in enumerate(hand_b):
        d.append(dw.Text(c, font_size=scale*.75, x=gx(-2)+scale*.05, y=gy(i*0.8), **char_property))
        if i >= 11:
            break
    if len(hand_b) >= 12:
        for i, c in enumerate(hand_b[12:]):
            d.append(dw.Text(c, font_size=scale*.75, x=gx(-1.2)+scale*.05, y=gy((i+5)*0.8), **char_property))
            if i >= 18:
                break
        if len(hand_b) >= 18:
            d.append(dw.Text('他', font_size=scale*.75, x=gx(-0.35), y=gy(5*0.8), **char_property))
    for i, c in enumerate(hand_w):
        d.append(dw.Text(c, font_size=scale*.75, x=gx(11)+scale*.5, y=gy(8.4-i*0.8), rotate=180, **char_property))
        if i >= 11:
            break
    if len(hand_w) >= 12:
        for i, c in enumerate(hand_w[12:]):
            d.append(dw.Text(c, font_size=scale*.75, x=gx(10.2)+scale*.5, y=gy(8.4-(i+5)*0.8), rotate=180, **char_property))
        if len(hand_w) >= 18:
            d.append(dw.Text('他', font_size=scale*.75, x=gx(9.9), y=gy(5*0.8), **char_property))

    return d

                
class State(minioslcc.CCState):
    """shogi state = board position + pieces in hand (mochigoma), enhanced with move history"""
    def __init__(self, init=''):
        super().__init__()
        self.history = []
        self.last_move_ja = None
        self.id = random.randrange(2**20)
        self.prefer_svg = is_in_notebook()
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
        return csaboard_to_svg(self, self.id, decorate=decorate)

    def to_png(self, *, decorate=False) -> drawsvg.raster.Raster:
        return self.to_svg(decorate=decorate).rasterize()

    def hint(self):
        return self.to_svg() if self.prefer_svg else self.to_csa()
    
    def make_move(self, move):
        if isinstance(move, str):
            move = self.to_move(move)
            if not move.is_normal():
                raise ValueError('please check syntax '+move)
        self.last_move_ja = self.move_to_ja(move) # need to be here before make_move
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
        return [ self.move_to_ja(move) for move in self.genmove() ]

    def last_move(self) -> minioslcc.Move:
        return self.history[-1] if len(self.history) > 0 else None
    
    def last_to(self) -> minioslcc.Square:
        move = self.last_move()
        return move.dst() if move else None
    
    def clone(self) -> State:
        return State(self.to_usi())

    def _replay(self):
        if self.to_usi() != self.initial_state:
            super().reset(minioslcc.usi_board(self.initial_state))
        if len(self.history) == 0:
            return
        self.last_move_ja = None
        for i, move in enumerate(self.history):
            if i == len(self.history)-2: # just before last make_move
                if i > 0:
                    self.last_move_ja = minioslcc.move_to_ja(self, move, history[i-1].to())
                else:
                    self.last_move_ja = minioslcc.move_to_ja(self, move) 
            super().make_move(move)


def minirecord_replay(self: minioslcc.MiniRecord, n: int) -> State:
    s = State(self.initial_state)
    for i, move in enumerate(self.moves):
        if i >= n:
            break
        s.make_move(move)
    return s

def minirecord_to_usi(self: minioslcc.MiniRecord) -> str:
    s = self.initial_state.to_usi()
    if len(self.moves) > 0:
        s += ' moves ' + ' '.join([move.to_usi() for move in self.moves])
    return s

setattr(minioslcc.MiniRecord, 'replay', minirecord_replay)
setattr(minioslcc.MiniRecord, 'to_usi', minirecord_to_usi)
