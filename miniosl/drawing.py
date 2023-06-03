from __future__ import annotations
import minioslcc
import drawsvg as dw
# import cairosvg

css = ".piece { font-family: Noto Serif CJK JP; }"  # prevent from tofu in png

csadict = {
    'FU': '歩', 'KY': '香', 'KE': '桂', 'GI': '銀', 'KI': '金', 'KA': '角', 'HI': '飛', 'OU': '王',
    'TO': 'と', 'NY': '杏', 'NK': '圭', 'NG': '全', 'UM': '馬', 'RY': '龍',
}
kanjirow = [None, '一', '二', '三', '四', '五', '六', '七', '八', '九']
kanjicol = [None, '1', '2', '3', '4', '5', '6', '7', '8', '9']

no_pieces_in_hand = 'なし'
label_hand_b = '先手持駒'
label_hand_w = '後手持駒'
hand_others = '他'

char_property = {'class': "piece", 'fill': 'black'}
sub_property = {'class': "piece", 'fill': 'gray'}
b_property = {'fill': 'orange', 'fill_opacity': 0.2, 'stroke_width': 0}
w_property = {'fill': 'blue', 'fill_opacity': 0.2, 'stroke_width': 0}
c_property = {'fill': 'purple', 'fill_opacity': 0.2, 'stroke_width': 0}
in_check_property = {'fill': 'red', 'fill_opacity': 0.5, 'stroke_width': 0}
last_move_property = {'fill': 'yellow', 'fill_opacity': 0.2, 'stroke_width': 0}
scale = 20


def gx(x):
    return (11-x)*scale+scale/2


def gy(y):
    return (y+1)*scale


def radius(cnt):
    return scale/4 * (cnt ** 1/4)


class ShogiSVG:
    """svg under drawing"""

    def __init__(self, state: State):
        self.state = state
        self.d = dw.Drawing(scale*14.5, scale*11.5, id_prefix=str(id))
        self.d.append_css(css)

    def add(self, element) -> None:
        self.d.append(element)

    def _decorate_king(self, player: minioslcc.Player) -> None:
        prop = b_property if player == minioslcc.black else w_property
        if self.state.turn() == player and self.state.in_check():
            prop = in_check_property
        else:
            prop = prop.copy()
            prop['fill_opacity'] = 0.1
        x, y = self.state.king_square(player).to_xy()
        self.add(dw.Rectangle(gx(x), gy(y-1), scale, scale, **prop))

    def decorate(self):
        self._decorate_king(minioslcc.black)
        self._decorate_king(minioslcc.white)
        for x in range(1, 10):
            for y in range(1, 10):
                eb = self.state.count_cover(minioslcc.black, minioslcc.Square(x, y))
                ew = self.state.count_cover(minioslcc.white, minioslcc.Square(x, y))
                if eb > 0 and ew > 0:
                    self.add(dw.Circle(gx(x-.5), gy(y-.5), radius(eb+ew), **c_property))
                elif eb > 0:
                    self.add(dw.Circle(gx(x-.5), gy(y-.5), radius(eb), **b_property))
                elif ew > 0:
                    self.add(dw.Circle(gx(x-.5), gy(y-.5), radius(ew), **w_property))
    
    def draw_piece(self, x, y):
        piece = self.state.piece_at(minioslcc.Square(x, y))
        if not piece.is_piece():
            return
        kanji = csadict[minioslcc.to_csa(piece.ptype())]
        if piece.color() == minioslcc.black:
            self.add(dw.Text(kanji, font_size=scale*.875, x=gx(x)+scale*.05, y=gy(y)-scale*.125,
                             **char_property))
        else:
            assert piece.color() == minioslcc.white
            self.add(dw.Text(kanji, font_size=scale*.875, x=gx(x)+scale*.9, y=gy(y)-scale*.8,
                             rotate='180', **char_property))

    def draw_grid(self):
        for i in range(1, 10):
            self.add(dw.Text(kanjicol[i], font_size=scale*.75, x=gx(i)+scale*.25, y=gy(0)-scale*0.25,
                             **sub_property))
            self.add(dw.Text(kanjirow[i], font_size=scale*.625, x=gx(0)+scale/4, y=gy(i)-scale*0.25,
                             **sub_property))
            if i < 9:
                self.add(dw.Line(gx(0), gy(i), gx(9), gy(i), stroke='gray'))
                self.add(dw.Line(gx(i), gy(0), gx(i), gy(9), stroke='gray'))
        self.add(dw.Line(gx(0), gy(0), gx(9), gy(0), stroke='black'))
        self.add(dw.Line(gx(0), gy(9), gx(9), gy(9), stroke='black'))
        self.add(dw.Line(gx(0), gy(0), gx(0), gy(9), stroke='black'))
        self.add(dw.Line(gx(9), gy(0), gx(9), gy(9), stroke='black'))

    def show_side_to_move(self):
        player_to_move = "先手" if self.state.turn() == minioslcc.black else "後手"
        self.add(dw.Text('手番 '+player_to_move, font_size=scale*.75, x=gx(9)+scale*.05, y=gy(10),
                         **char_property))
        if self.state.last_move_ja:
            self.add(dw.Text('('+self.state.last_move_ja + ' まで)', font_size=scale*.7,
                             x=gx(5)+scale*.05, y=gy(10), **sub_property))

    def hand_pieces_str(self, player: minioslcc.Player) -> str:
        ret = ''
        for ptype in minioslcc.piece_stand_order:
            cnt = self.state.count_hand(player, ptype)
            if cnt == 0:
                continue
            ret += csadict[minioslcc.to_csa(ptype)] * cnt
        return ret

    def show_hand(self):
        # i: [0,4] label+space
        hand_b = self.hand_pieces_str(minioslcc.black)
        hand_w = self.hand_pieces_str(minioslcc.white)
        prop = sub_property.copy()
        prop['font_size'] = scale*.75
        for i in range(len(label_hand_b)):
            self.add(dw.Text(label_hand_b[i], x=gx(-2)+scale*.05, y=gy(i*0.8),
                             **prop))
            self.add(dw.Text(label_hand_w[i], x=gx(11)+scale*.5, y=gy(8.4-i*0.8),
                             rotate=180, **prop))
        if not hand_b:
            for i, c in enumerate(no_pieces_in_hand):
                self.add(dw.Text(c, x=gx(-2)+scale*.05, y=gy((i+5)*0.8),
                                 **prop))
        if not hand_w:
            for i, c in enumerate(no_pieces_in_hand):
                self.add(dw.Text(c, x=gx(11)+scale*.5, y=gy(8.4-(i+5)*0.8),
                                 rotate=180, **prop))

        prop = char_property.copy()
        prop['font_size'] = scale*.75
        for i in range(min(7, len(hand_b))):
            self.add(dw.Text(hand_b[i], x=gx(-2)+scale*.05, y=gy((i+5)*0.8),
                             **prop))
        for i in range(7, min(14, len(hand_b))):
            self.add(dw.Text(hand_b[i], x=gx(-1.2)+scale*.05, y=gy((i-2)*0.8),
                             **prop))
        if len(hand_b) >= 14:
            self.add(dw.Text(hand_others, x=gx(-0.35), y=gy(5*0.8), **prop))
        for i in range(min(7, len(hand_w))):
            self.add(dw.Text(hand_w[i], x=gx(11)+scale*.5, y=gy(8.4-(i+5)*0.8),
                             rotate=180, **prop))
        for i in range(7, min(14, len(hand_w))):
            self.add(dw.Text(hand_w[i], x=gx(10.2)+scale*.5, y=gy(8.4-(i-2)*0.8),
                             rotate=180, **prop))
        if len(hand_w) >= 14:
            self.add(dw.Text(hand_others, x=gx(9.9), y=gy(5*0.8), **prop))


def csaboard_to_svg(state: State, id: int, *, decorate=False) -> drawsvg.drawing.Drawing:
    svg = ShogiSVG(state)
    if decorate:
        svg.decorate()

    if last_to := state.last_to():
        x, y = last_to.to_xy()
        svg.add(dw.Rectangle(gx(x), gy(y-1), scale, scale, **last_move_property))

    for x in range(1, 10):
        for y in range(1, 10):
            svg.draw_piece(x, y)

    svg.draw_grid()
    svg.show_side_to_move()
    svg.show_hand()

    return svg.d
