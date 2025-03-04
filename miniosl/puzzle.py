import miniosl
import json
import enum
import os.path

Status = enum.Enum('Status', [
    ('Playing', 0),
    ('Solved', 1),
    ('Failed', 2),
])


class Puzzle(miniosl.UI):
    def __init__(self, puzzle):
        self.puzzle = puzzle
        super().__init__(puzzle['board'])
        self.move_cache = self._gen_puzzle_move()
        self.status = [Status.Playing]

    def _add_drawing_properties(self, dict):
        super()._add_drawing_properties(dict)
        for key in ['xlim', 'ylim']:
            dict[key] = self.puzzle[key]

    def reset(self):
        self.load_record(self.puzzle['board'])
        self.move_cache = self._gen_puzzle_move()
        self.status = [Status.Playing]

    def make_move(self, move):
        ret = super().make_move(move)
        self.move_cache = self._gen_puzzle_move()
        self.status.append(self.new_status())
        return ret

    def unmake_move(self):
        super().unmake_move()
        self.move_cache = self._gen_puzzle_move()
        self.status.pop(-1)

    def new_status(self):
        if self.puzzle['mate']:
            side_to_move = self.turn()
            if side_to_move == miniosl.white:
                if self.in_checkmate():
                    return Status.Solved
                if not self.in_check():
                    return Status.Failed
            else:
                if len(self.move_cache) == 0:
                    return Status.Failed
        return Status.Playing

    @property
    def xlim(self):
        return self.puzzle['xlim']

    @property
    def ylim(self):
        return self.puzzle['ylim']

    def in_puzzle_area(self, sq):
        return sq.x < self.xlim and sq.y < self.ylim

    def _gen_puzzle_move(self):
        side_to_move = self.turn()
        if side_to_move == miniosl.white:
            return self.genmove()
        moves = self.genmove_check()
        return [_ for _ in moves if self.in_puzzle_area(_.dst)]

    def comment(self):
        if 'comment' in self.puzzle:
            path = ''.join([_.to_csa() for _ in self.history()])
            if specific := self.puzzle['comment'].get(path, ''):
                return specific
        num_moves = len(self.move_cache)
        if self.puzzle['mate']:
            side_to_move = self.turn()
            if side_to_move == miniosl.white:
                if self.in_checkmate():
                    return '詰みました (成功)'
                if not self.in_check():
                    return '王手ではありません (失敗)'
                if num_moves == 1 and self.move_cache[0].is_capture():
                    return '玉方は取る一手です'
                return f'{num_moves}通りの応手があります'
            else:
                if num_moves == 0:
                    return 'もう王手をかけられません (失敗)'
                return f'{num_moves}通りの王手があります'
        else:
            return 'not implemented'
        return ''

    def auto_reply(self):
        if not self.model:
            self.load_eval()
        mp = self.gumbel_one()
        return mp[0][1]


def load_json_file(path):
    with open(path) as f:
        return json.load(f)


def tsumeshogi_play(name=''):
    if not name:
        name = 'puzzle1'
    puzzle = None
    if os.path.exists(name):
        puzzle = load_json_file(name)
    else:
        path = miniosl.puzzle_path().joinpath(name + '.json')
        if os.path.exists(path):
            puzzle = load_json_file(path)
    if not puzzle:
        raise ValueError('data not found')
    return Puzzle(puzzle)


def tsumeshogi_widget(name=''):
    import miniosl.widget
    puzzle = tsumeshogi_play(name)
    return miniosl.widget.PuzzleWindow(puzzle)
