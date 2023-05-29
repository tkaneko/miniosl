import miniosl
import minioslcc

def test_state():
    board = miniosl.State()
    svg = board.to_svg()
    b2 = miniosl.State('sfen lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/P8/1PPPPPPPP/1B5R1/LNSGKGSNL b - 1')


def test_make_move():
    board = miniosl.State()
    board.make_move('7g7f')
    assert board.last_to().to_xy() == (7,6)
    svg = board.to_svg()
    board.make_move('-3334FU')
    assert board.last_to().to_xy() == (3,4)
    svg = board.to_svg()
    svg = board.to_svg(decorate=True)


def test_genmove():
    board = miniosl.State()
    moves = board.genmove()
    assert '7g7f' in [ m.to_usi() for m in moves]
    assert '+7776FU' in [ m.to_csa() for m in moves]
    assert len(moves) == 30
    copy = board.clone()
    board.make_move('+7776FU')
    moves = board.genmove()
    assert '+7776FU' not in [ m.to_csa() for m in moves]
    moves = copy.genmove()
    assert '+7776FU' in [ m.to_csa() for m in moves]
    board.unmake_move()
    moves = board.genmove()
    assert '7g7f' in [ m.to_usi() for m in moves]
    
    
    
def test_initial_csa():
    board = minioslcc.CCState()
    csa = board.to_csa()
    for r,line in enumerate(csa.split('\n')):
        if r < 9:
            assert len(line) == 2+3*9
            assert line[0:2] == 'P'+str(r+1)
        else:
            assert line == '+' or line == '-' or len(line) < 1


def test_replay():
    board = miniosl.State()
    board.make_move('+7776FU')
    board.make_move('-3334FU')

def test_bitset():
    mobility = miniosl.ptype_move_direction[miniosl.knight]
    one_hot = 2**int(miniosl.UUR)
    assert (mobility & one_hot) != 0
