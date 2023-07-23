import curses
import miniosl
import argparse

parser = argparse.ArgumentParser(
    description="terminal ui for miniosl",
    formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument("--sfen", help="file to show")
parser.add_argument("--eval", help="optional evaluation function")
parser.add_argument("--device", help="device")
parser.add_argument("--inspect-terminal", help="terminal property",
                    action="store_true")
args = parser.parse_args()


class TUI:
    def __init__(self, ui, record_set):
        self.UI = ui
        self.record_set = record_set
        self.record_id = 0
        self.tree = None
        N = len(record_set.records)
        if N > 0:
            ui.load_record(record_set.records[0])
            limit = 2 if N < 40000 else 10
            self.tree = miniosl.OpeningTree.from_record_set(record_set, limit)

    def tx(x):
        return (9-x) * 3 + 2*3

    def ty(y):
        return y + 3

    def color_pair_plb():
        return curses.color_pair(2)

    def color_pair_plw():
        return curses.color_pair(3)

    def color_pair_bg():
        return curses.color_pair(4)

    def __call__(self, stdscr):
        self.scr = stdscr
        self.scr.clear()
        board_color = curses.COLOR_YELLOW
        if curses.can_change_color():
            curses.init_color(curses.COLORS-1, 300, 300, 300)
            board_color = curses.COLORS-1
        curses.curs_set(False)
        curses.init_pair(2, curses.COLOR_WHITE, curses.COLOR_BLACK)
        curses.init_pair(3, curses.COLOR_BLACK, curses.COLOR_WHITE)
        curses.init_pair(4, curses.COLOR_BLACK, board_color)

        quit = False
        while not quit:
            self.showboard()
            self.scr.refresh()
            key = self.scr.getkey()
            quit = key == 'q' or key == 'Q'
            self.clear_status()
            if key == 'u':
                self.show_status(self.UI._state.to_usi())
                continue
            if key == '>':
                self.show_status('last position')
                self.UI.last()
                continue
            if key == '<':
                self.show_status('first position')
                self.UI.first()
                continue
            step = -1 if key == curses.KEY_LEFT or key == curses.KEY_UP or key == 'b' else 1
            if len(self.record_set.records) == 0:
                quit = True
            elif 0 <= self.UI.cur+step <= self.record_len():
                self.show_status(f'step {step:+}')
                self.UI.go(step)
            elif 0 <= self.record_id + step < len(self.record_set.records):
                self.record_id += step
                self.show_status(f'load record {self.record_id}')
                self.UI.load_record(self.record_set.records[self.record_id])
                if step < 0:
                    self.UI.last()
            else:
                quit = True

    def record_len(self):
        return len(self.record_set.records[self.record_id])

    def piece_to_ja(self, piece):
        if not piece.is_piece():
            return '　'
        return miniosl.ptype_to_ja(piece.ptype())

    def draw_piece(self, x, y, piece):
        kanji = self.piece_to_ja(piece)
        tx, ty = TUI.tx(x), TUI.ty(y)
        if not piece.is_piece():
            self.scr.addstr(ty, tx, " "+kanji, TUI.color_pair_bg())
        elif piece.color() == miniosl.white:
            self.scr.addstr(ty, tx, "v", TUI.color_pair_plw())
            self.scr.addstr(ty, tx+1, kanji, TUI.color_pair_plw())
        else:
            self.scr.addstr(ty, tx, " ", TUI.color_pair_bg())
            self.scr.addstr(ty, tx+1, kanji, TUI.color_pair_plb())

    def showboard(self):
        for y in range(1, 10):
            for x in range(1, 10):
                piece = self.UI._state.piece_at(miniosl.Square(x, y))
                self.draw_piece(x, y, piece)
        hand_b, hand_w = [miniosl.hand_pieces_to_ja(self.UI._state, _)+" "*20
                          for _ in (miniosl.black, miniosl.white)]
        self.scr.addstr(TUI.ty(-1), TUI.tx(9), "後手持駒 "+hand_w, TUI.color_pair_plb())
        self.scr.addstr(TUI.ty(11), TUI.tx(9), "先手持駒 "+hand_b)
        self.scr.addstr(TUI.ty(12), TUI.tx(9), f'第{self.record_id+1}局'
                        + f' (全{len(self.record_set.records)}局)')
        msg = f'{self.UI.cur+1}/{self.record_len()}手目'
        if self.UI.last_move_ja:
            msg += f' ({self.UI.last_move_ja}まで)'
            if self.UI._state.in_checkmate():
                msg += ' 詰み'
            msg += " "*40
        else:
            msg += " "*80
        self.scr.addstr(TUI.ty(13), TUI.tx(9), msg, curses.color_pair(2))
        self.clear_opening()
        if self.tree:
            self.show_opening()
        self.clear_eval()
        if self.UI.model and not self.UI._state.in_checkmate():
            self.show_eval()

    def clear_status(self):
        self.scr.addstr(TUI.ty(15), TUI.tx(10), " "*80)

    def show_status(self, msg):
        self.scr.addstr(TUI.ty(15), TUI.tx(10), msg)

    def clear_opening(self):
        for i in range(3):
            self.scr.addstr(TUI.ty(i), TUI.tx(-4), " "*24)

    def show_opening(self):
        all = len(self.record_set.records)
        children = self.tree.retrieve_children(self.UI._state)
        self.scr.addstr(TUI.ty(-1), TUI.tx(-2), '統計')
        for i, c in enumerate(children):
            if i > 2:
                break
            move = miniosl.to_ja(c[1], self.UI._state)
            self.scr.addstr(TUI.ty(i), TUI.tx(-4), f'{move:8s}')
            self.scr.addstr(TUI.ty(i), TUI.tx(-8), f'{c[0].count()/all*100:5.1f}%')

    def clear_eval(self):
        for i in range(4):
            self.scr.addstr(TUI.ty(i+5), TUI.tx(-4), " "*24)
        for i in range(4):
            self.scr.addstr(TUI.ty(i+10), TUI.tx(-4), " "*40)

    def show_eval(self):
        value, mp = self.UI.eval(verbose=False)
        self.scr.addstr(TUI.ty(4), TUI.tx(-2), f'NN {args.eval}')
        self.scr.addstr(TUI.ty(5), TUI.tx(-4), f'評価値 {value:5.0f}')
        for i in range(min(len(mp), 3)):
            self.scr.addstr(TUI.ty(6+i), TUI.tx(-4),
                            f'{miniosl.to_ja(mp[i][1], self.UI._state):9s}')
            self.scr.addstr(TUI.ty(6+i), TUI.tx(-8),
                            f'{mp[i][0]*100:5.1f}%')
        self.scr.addstr(TUI.ty(9), TUI.tx(-2), 'one-ply')
        values = self.UI.gumbel_one()
        best = values[0][0]
        for i, item in enumerate(values):
            move = item[1]
            diff = item[0] - item[2]
            cp = TUI.color_pair_plb() if item[0]+4 > best else TUI.color_pair_bg()
            self.scr.addstr(TUI.ty(10+i), TUI.tx(-4),
                            f'{miniosl.to_ja(move, self.UI._state):9s}', cp)
            self.scr.addstr(TUI.ty(10+i), TUI.tx(-8),
                            f'{item[0]:5.1f} ({diff:+5.1f}) {item[3]:+.3f}', cp)


def main():
    if args.inspect_terminal:
        _ = curses.initscr()
        curses.start_color()
        curses.endwin()
        print(curses.can_change_color())
        print(curses.COLORS)
        exit()
    record_set = miniosl.RecordSet()
    if args.sfen:
        if args.sfen.endswith('.npz'):
            record_set = miniosl.RecordSet.from_npz(args.sfen)
        else:
            record_set = miniosl.RecordSet.from_usi_file(args.sfen)
    ui = miniosl.UI()
    if args.eval:
        ui.load_eval(args.eval, device=args.device)
    curses.wrapper(TUI(ui, record_set))
