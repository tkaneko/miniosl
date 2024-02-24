import curses
import curses.panel
import curses.textpad
import miniosl
import argparse
import os
import os.path
import datetime
import numpy as np

parser = argparse.ArgumentParser(
    description="terminal ui for miniosl",
    formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument("--sfen", help="file to show")
parser.add_argument("--eval", help="optional evaluation function",
                    default=os.path.expanduser(miniosl.pretrained_eval_path())
                    )
parser.add_argument("--device", help="device", default='cpu')
parser.add_argument("--inspect-terminal", help="terminal property",
                    action="store_true")
args = parser.parse_args()
threat_indicator = '(詰筋)'


class Color:
    def sente():
        return curses.color_pair(2)

    def gote():
        return curses.color_pair(3)

    def board():
        return curses.color_pair(4)

    def highlight():
        return curses.color_pair(5)

    def editor():
        return Color.sente()

    def init_color():
        if curses.COLORS >= 256:
            board_color = 255 - 33
            piece_fg_black = 15
            piece_bg_black = 94
            piece_fg_white = curses.COLOR_BLACK
            piece_bg_white = 255 - 40  # - 63 +16  # 35
        else:
            board_color = curses.COLOR_YELLOW
            piece_fg_black = curses.COLOR_WHITE
            piece_bg_black = curses.COLOR_BLACK
            piece_fg_white = curses.COLOR_BLACK
            piece_bg_white = curses.COLOR_WHITE
        curses.init_pair(2, piece_fg_black, piece_bg_black)
        curses.init_pair(3, piece_fg_white, piece_bg_white)
        curses.init_pair(4, curses.COLOR_BLACK, board_color)
        curses.init_pair(5, curses.COLOR_BLACK, curses.COLOR_WHITE)
        curses.init_pair(6, curses.COLOR_BLACK, board_color)


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
        self.state_shown = None

    def _init_screen(self, stdscr):
        self.scr = stdscr
        self.scr.clear()
        board_width = 29
        board_offset = 2
        self.board = self.scr.derwin(11, board_width, 4-1, board_offset)
        self.board.border()
        self.status = self.scr.subwin(1, curses.COLS, curses.LINES-1, 0)
        left_width = board_width+board_offset + 8
        self.hand_b = self.scr.subwin(1, left_width, 14, board_offset+1)
        self.hand_w = self.scr.subwin(1, left_width, 2, board_offset+1)
        self.headline = self.scr.subwin(1, curses.COLS, 0, 0)
        self.board_status = self.scr.subwin(1, left_width, 15, board_offset)
        self.opening_panel = self.scr.subwin(4, curses.COLS - left_width - 1,
                                             2, left_width)
        self.policy_panel = self.scr.subwin(10, curses.COLS - left_width - 1,
                                            6, left_width)
        self.main_panel = self.scr.subwin(curses.LINES - 18, curses.COLS - 2,
                                          17, 1)
        self.main_panel.idlok(True)
        self.main_panel.scrollok(True)

        self.helpwin = curses.newwin(curses.LINES//2, curses.COLS//2, 4, 4)
        self.help_panel = curses.panel.new_panel(self.helpwin)
        self.helpwin.addstr(1, 0,
                            ' h: show this help\n'
                            ' q: quit\n'
                            ' n, f: next state\n'
                            ' p, b: prev state\n'
                            ' > (<): last (first) state\n'
                            ' s: run mcts\n'
                            ' u: show board in usi\n'
                            ' c: show board in csa\n'
                            )
        self.helpwin.border()
        self.help_panel.hide()

        self.record_dialogue = curses.newwin(1, 10,
                                             curses.LINES-1,
                                             min(curses.COLS//4, 12))
        self.record_dialogue.bkgd(' ', Color.editor())
        self.recordd_panel = curses.panel.new_panel(self.record_dialogue)
        self.recordd_editor = curses.textpad.Textbox(self.record_dialogue)
        self.recordd_panel.hide()

        Color.init_color()
        curses.curs_set(False)

    def _show_alt_pv(self, search_root, move, ratio=0.25):
        if move in search_root.children:
            alt_child = search_root.children[move]
        elif move in search_root.loss_children:
            alt_child = search_root.loss_children[move]
        else:
            return
        if alt_child.visit_count <= 1:
            return
        alt_pv = alt_child.pv(ratio=ratio, sign=1)
        alt_jpv = self.UI.pv_to_ja([move]
                                   + [_[0] for _ in alt_pv])
        threatmate_pv = [threat_indicator
                         if alt_child.in_threatmate() else ''] \
            + [threat_indicator if _[3] else '' for _ in alt_pv]
        pvt = [a + b for a, b in zip(alt_jpv, threatmate_pv)]
        self.show_lines(f'{1-alt_child.value():.3f}',
                        [f'{pvt[0]} '] + pvt[1:]
                        + [f' {alt_child.visit_count/1000:.1f}k'],
                        alt=True)

    def search(self, search_root):
        self.scr.nodelay(True)
        status_head = 'running mcts (press any key to stop)'
        self.show_status(status_head)
        if not search_root:
            self.budget_total = 0
            self.main_panel.clear()
            self.past_best_moves = set()
            self.last_best_move = None
        budget = 800
        budget_total_at_start = self.budget_total
        started = datetime.datetime.now()
        ratio = 0.25
        batch_size = 32 if self.UI.model.device == 'cuda' else 1
        while not search_root or not search_root.is_terminal():
            search_root = self.UI.mcts(budget=budget, report=0,
                                       batch_size=batch_size,
                                       resume_root=search_root)
            self.budget_total += budget
            pv = search_root.pv(ratio=ratio)
            if pv:
                jpv = self.UI.pv_to_ja([_[0] for _ in pv])
                m = pv[0]
                threatmate_pv = [threat_indicator if _[3] else '' for _ in pv]
                pvt = [a + b for a, b in zip(jpv, threatmate_pv)]
                first = pvt[0]
                self.show_lines(f'{m[1]:.3f}',
                                [f'{first} '] + pvt[1:]
                                + [f' {search_root.visit_count/1000:.1f}k'])
                if self.last_best_move and self.last_best_move != m[0]:
                    self._show_alt_pv(search_root, self.last_best_move)
                self.last_best_move = m[0]
                self.past_best_moves.add(m[0])
            now = datetime.datetime.now()
            elapsed = (now - started).total_seconds()
            nps = (self.budget_total - budget_total_at_start) / 1000 / elapsed
            self.clear_status(len(status_head))
            self.status.move(0, 2 + len(status_head) + 1)
            self.status.addstr(f'{elapsed:.1f}s nps={nps:.1f}k',
                               Color.highlight())
            self.status.refresh()
            if search_root.is_terminal():
                break
            keyin = self.scr.getch()
            if keyin != -1:
                break
        th = int(search_root.visit_count*ratio)
        children = search_root.visited_children(th)[:3]
        for move, _ in children:
            if move != self.last_best_move:
                self._show_alt_pv(search_root, move)
        for move in self.past_best_moves:
            if move != self.last_best_move \
               and move not in [_[0] for _ in children]:
                self._show_alt_pv(search_root, move)
        if search_root.in_threatmate():
            self.show_lines('-', threat_indicator)
        self.clear_status()
        if search_root.is_terminal():
            msg = 'game value identified'
        else:
            msg = 'mcts stopped (press s again to continue)'
        self.show_status(msg)
        self.scr.nodelay(False)
        return search_root

    def show_lines(self, head, msgs, alt=False):
        _, width = self.status.getmaxyx()
        width = width // 2 + 5
        indent = '  '
        cur = len(head)
        self.main_panel.addstr('\n')
        self.main_panel.addstr(head,
                               curses.color_pair(0) if alt else Color.sente())
        self.main_panel.addstr(' ')
        for word in msgs:
            if len(word)+1 >= width:
                word = '..'
            if cur + len(word) + 1 < width:
                self.main_panel.addstr(word)
                cur += len(word)
            else:
                self.main_panel.addstr('\n')
                self.main_panel.addstr(indent + word)
                cur = len(indent + word)
        self.main_panel.refresh()

    def __call__(self, stdscr):
        self._init_screen(stdscr)
        self.clear_status()
        self.show_status('started')
        self.update_headline()

        quit = False
        search_root = None
        while not quit:
            self.showboard()
            self.last_key = key = self.scr.getkey()
            quit = key in 'qQ'
            if quit:
                break
            self.clear_status()
            if key in 'uU':
                self.show_status(self.UI._state.to_usi())
                continue
            if key == '>':
                self.show_status('last position')
                self.UI.last()
                search_root = None
                continue
            if key == '<':
                self.show_status('first position')
                self.UI.first()
                search_root = None
                continue
            if key in 'cC':
                self.main_panel.clear()
                self.main_panel.addstr(self.UI.to_csa()[:-1])
                self.main_panel.refresh()
                continue
            if key in 'sS':
                search_root = self.search(search_root)
                continue
            if key in 'hH':
                self.helpwin.refresh()
                self.help_panel.show()
                self.scr.getch()
                self.help_panel.hide()
                continue
            if key in 'gG':
                self.show_status('numbper?')
                self.record_dialogue.clear()
                self.record_dialogue.refresh()
                self.recordd_panel.show()
                curses.curs_set(1)
                ret = self.recordd_editor.edit()
                curses.curs_set(0)
                ret = ret[:-1]
                self.recordd_panel.hide()
                self.clear_status()
                if (ret.isdigit()
                    and (0
                         < int(ret)
                         <= len(self.record_set.records))):
                    self.load_record(int(ret) - 1)
                else:
                    self.show_status('please enter digits for record id'
                                     f' (got {ret})')
                continue
            if key in ['KEY_NPAGE', 'KEY_PPAGE']:
                add = 10 if key == 'KEY_NPAGE' else -10
                new_id = add + self.record_id
                if 0 <= new_id < len(self.record_set.records):
                    self.load_record(new_id)
                continue
            search_root = None
            step = 1
            if key in ['KEY_UP', 'KEY_LEFT', 'b', 'B', 'p', 'P']:
                step = -1
            if key in ['KEY_UP', 'KEY_DOWN', ' ']:
                step *= 10
            if len(self.record_set.records) == 0:
                self.show_status('no games')
            elif 0 <= self.UI.cur+step <= self.record_len():
                self.show_status(f'step {step:+}')
                self.UI.go(step)
            elif (0 <= self.record_id + np.sign(step)
                  < len(self.record_set.records)):
                self.load_record(self.record_id + np.sign(step))
                if step < 0:
                    self.UI.last()
            else:
                self.show_status('no record available (press q for quit)')

    def load_record(self, new_id):
        self.record_id = new_id
        self.show_status(f'load record {self.record_id+1}')
        self.UI.load_record(self.record_set.records[self.record_id])
        self.UI.first()
        self.update_headline()

    def record_len(self):
        return len(self.record_set.records[self.record_id])

    def piece_to_ja(self, piece):
        if not piece.is_piece():
            return '　'
        return miniosl.ptype_to_ja(piece.ptype())

    def draw_piece(self, x, y, piece):
        kanji = self.piece_to_ja(piece)
        tx, ty = (9-x)*3 + 1, y  # 1 for boarder
        if not piece.is_piece():
            self.board.addstr(ty, tx, " "+kanji, Color.board())
        elif piece.color() == miniosl.white:
            self.board.addstr(ty, tx, "v", Color.gote())
            self.board.addstr(ty, tx+1, kanji, Color.gote())
        else:
            self.board.addstr(ty, tx, " ", Color.board())
            self.board.addstr(ty, tx+1, kanji, Color.sente())

    def update_headline(self):
        self.headline.clear()
        h_msg = f' 第{self.record_id+1}局' \
            + f' (全{len(self.record_set.records)}局) '
        self.headline.addstr(0, 0, h_msg, Color.highlight())
        program = os.path.basename(__file__) \
            + ' (miniosl ' + miniosl.version() + ')'
        if remain := max(curses.COLS - (len(h_msg)*2 + len(program)), 0):
            self.headline.addstr(' '*remain)
            self.headline.addstr(program)
        self.headline.refresh()

    def showboard(self):
        squares = []
        for y in range(1, 10):
            for x in range(1, 10):
                sq = miniosl.Square(x, y)
                piece = self.UI._state.piece_at(sq)
                if ((not self.state_shown
                     or not self.state_shown.piece_at(sq).equals(piece))):
                    squares.append(sq)
        for sq in squares:
            piece = self.UI._state.piece_at(sq)
            self.draw_piece(sq.x(), sq.y(), piece)
        hand_b, hand_w = [miniosl.hand_pieces_to_ja(self.UI._state, _)
                          for _ in (miniosl.black, miniosl.white)]
        if (not self.state_shown
            or (miniosl.hand_pieces_to_ja(self.state_shown, miniosl.black)
                != hand_b)):
            if not self.state_shown:
                self.hand_b.addstr(0, 0, "先手 ")
            else:
                self.hand_b.move(0, 5)
                self.hand_b.clrtoeol()
            self.hand_b.addstr(hand_b, Color.sente())
            self.hand_b.refresh()
        if (not self.state_shown
            or (miniosl.hand_pieces_to_ja(self.state_shown, miniosl.white)
                != hand_w)):
            if not self.state_shown:
                self.hand_w.addstr(0, 0, "後手 ")
            else:
                self.hand_w.move(0, 5)
                self.hand_w.clrtoeol()
            self.hand_w.addstr(hand_w, Color.gote())
            self.hand_w.refresh()
        self.state_shown = miniosl.State(self.UI._state)
        self.board_status.clear()
        msg = f'{self.UI.cur+1} / {self.record_len()}手目'
        if self.UI.last_move_ja:
            msg += f' ({self.UI.last_move_ja}まで)'
            if self.UI._state.in_checkmate():
                msg += ' 詰み'
        self.board_status.addstr(0, 1, msg)
        self.board_status.refresh()
        self.clear_opening()
        if self.tree:
            self.show_opening()
        self.clear_eval()
        if self.UI.model and not self.UI._state.in_checkmate():
            self.show_eval()
        self.board.refresh()
        # self.scr.refresh()

    def clear_status(self, x: int = 0):
        self.status.move(0, x)
        self.status.clrtoeol()
        _, width = self.status.getmaxyx()
        self.status.addstr(' '*(width-1-x), Color.highlight())
        self.status.refresh()

    def show_status(self, msg):
        width = curses.COLS
        self.status.addstr(0, 2, msg[:width-1], Color.highlight())
        if hasattr(self, 'last_key'):
            lmsg = ' key = ' + self.last_key
            if len(msg) + len(lmsg) + 2 < width:
                self.status.addstr(0, curses.COLS - len(lmsg) - 2,
                                   lmsg,
                                   Color.highlight())
        self.status.refresh()

    def clear_opening(self):
        self.opening_panel.move(1, 0)
        self.opening_panel.clrtobot()

    def show_opening(self):
        all = len(self.record_set.records)
        children = self.tree.retrieve_children(self.UI._state)
        self.opening_panel.addstr(0, 0, '統計')
        for i, c in enumerate(children):
            if i > 2:
                break
            move = miniosl.to_ja(c[1], self.UI._state)
            self.opening_panel.addstr(i+1, 4, f'{move:11s}')
            self.opening_panel.addstr(f'{c[0].count()/all*100:5.1f}%')
        self.opening_panel.refresh()

    def clear_eval(self):
        self.policy_panel.move(1, 0)
        self.policy_panel.clrtobot()

    def show_eval(self):
        value, mp = self.UI.eval(verbose=False)
        self.policy_panel.addstr(0, 0, f'NN {os.path.basename(args.eval)}')
        self.policy_panel.addstr(1, 4, f'評価値 {value:5.0f}')
        for i in range(min(len(mp), 3)):
            mp_ja = miniosl.to_ja(mp[i][1], self.UI._state)
            self.policy_panel.addstr(2+i, 4, f'{mp_ja:11s}')
            self.policy_panel.addstr(2+i, 18, f'{mp[i][0]*100:5.1f}%')
        self.policy_panel.addstr(5, 0, 'one-ply')
        values = self.UI.gumbel_one()
        best = values[0][0]
        for i, item in enumerate(values):
            move = miniosl.to_ja(item[1], self.UI._state)
            diff = item[0] - item[2]
            cp = Color.sente() \
                if item[0]+4 > best else curses.color_pair(0)
            self.policy_panel.addstr(6+i, 4,
                                     f'{move:9s}', cp)
            self.policy_panel.addstr(6+i, 18,
                                     f'{item[0]:5.1f} ({diff:+5.1f}) '
                                     f'{item[3]:+.3f}',
                                     cp)
        self.policy_panel.refresh()


def inspect(stdscr):
    cols = min(16, curses.COLS)
    fg = curses.COLOR_BLACK
    # curses.COLOR_WHITE
    win = stdscr.subwin(curses.COLORS // cols + 1 + 1, cols + 2, 1, 1)
    for i in range(curses.COLORS):
        curses.init_pair(i+1, fg, i)
        win.addstr((i // cols + 1),
                   (i % cols + 1),
                   'a',
                   curses.color_pair(i+1)
                   )
    win.border()
    win.refresh()
    stdscr.getkey()


def main():
    if args.inspect_terminal:
        try:
            ret = curses.wrapper(inspect)
            if ret:
                print(f'{ret=}')
        except Exception as e:
            print(f'{type(e)=}, error: {e}')
        # _ = curses.initscr()
        # curses.start_color()
        # curses.endwin()
        print(f'{curses.can_change_color()=}')
        print(f'{curses.COLORS=}')
        print(f'{curses.COLS=}')
        print(f'{curses.LINES=}')
        exit()
    record_set = miniosl.RecordSet(
        miniosl.MiniRecordVector([miniosl.MiniRecord()])
    )
    if args.sfen:
        if args.sfen.endswith('.npz'):
            record_set = miniosl.RecordSet.from_npz(args.sfen)
        else:
            record_set = miniosl.RecordSet.from_usi_file(args.sfen)
    ui = miniosl.UI()
    if not os.path.exists(args.eval):
        args.eval = ''
    ui.load_eval(args.eval, device=args.device)
    try:
        curses.wrapper(TUI(ui, record_set))
    except Exception as e:
        import traceback
        print(f'error: {e}')
        print(traceback.format_exc())
        print('please run in terminals with curses available,'
              f' (current TERM is {os.environ["TERM"]})')
