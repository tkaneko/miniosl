import miniosl
import minioslcc
import numpy as np
import pytest

sfen = 'sfen lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/P8/1PPPPPPPP/1B5R1/LNSGKGSNL b - 1'


def test_state():
    base = minioslcc.CCState()
    assert isinstance(base, miniosl.CCState)
    assert isinstance(base, miniosl.BaseState)
    assert base.count_hand(miniosl.black, miniosl.pawn) == 0
    assert base.count_hand(miniosl.white, miniosl.pawn) == 0
    assert base.turn() == miniosl.black

    board = miniosl.State()
    assert isinstance(board, miniosl.State)
    assert isinstance(board, minioslcc.CCState)
    assert isinstance(board, minioslcc.BaseState)
    assert base.count_hand(miniosl.black, miniosl.pawn) == 0
    assert base.count_hand(miniosl.white, miniosl.pawn) == 0
    assert base.turn() == miniosl.black
    b2 = miniosl.State(sfen)
    assert b2


def test_make_move():
    board = miniosl.State()
    board.make_move('7g7f')
    copied = miniosl.State(board)
    copied2 = board.copy()
    assert board.turn() == miniosl.white
    assert board.to_usi() == copied.to_usi()
    assert board.to_usi() == copied2.to_usi()
    assert board.to_csa() == copied.to_csa()
    assert board.last_to().to_xy() == (7, 6)
    board.make_move('-3334FU')
    assert board.turn() == miniosl.black
    assert board.last_to().to_xy() == (3, 4)
    assert board.to_usi() != copied.to_usi()
    assert board.to_usi() != copied2.to_usi()
    board.make_move('+8822UM')
    assert board.turn() == miniosl.white
    assert board.count_hand(miniosl.black, miniosl.pawn) == 0
    assert board.count_hand(miniosl.black, miniosl.bishop) == 1


def test_genmove():
    board = miniosl.State()
    moves = board.genmove()
    assert '7g7f' in [m.to_usi() for m in moves]
    assert '+7776FU' in [m.to_csa() for m in moves]
    assert len(moves) == 30
    copy = board.copy()
    board.make_move('+7776FU')
    moves = board.genmove()
    assert '+7776FU' not in [m.to_csa() for m in moves]
    moves = copy.genmove()
    assert '+7776FU' in [m.to_csa() for m in moves]
    board.unmake_move()
    moves = board.genmove()
    assert '7g7f' in [m.to_usi() for m in moves]


def test_to_np():
    board = miniosl.State()
    feature = board.to_np()
    assert isinstance(feature, np.ndarray)
    assert feature.shape == (9, 9)


def test_to_np_hand():
    board = miniosl.State()
    feature = board.to_np_hand()
    assert isinstance(feature, np.ndarray)
    assert feature.shape == (2, 7)


def test_to_np_cover():
    board = miniosl.State()
    feature = board.to_np_cover()
    assert isinstance(feature, np.ndarray)
    assert feature.shape == (2, 9, 9)


def test_np_pack():
    board = miniosl.State()
    binary = board.to_np_pack()
    assert isinstance(binary, np.ndarray)
    assert binary.shape == (4,)
    assert binary.dtype == np.uint64


def test_board_csa():
    board = minioslcc.CCState()
    csa = board.to_csa()
    for r, line in enumerate(csa.split('\n')):
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


def test_encode_move():
    board = miniosl.State()
    board.make_move('+7776FU')
    board.make_move('-3334FU')
    board.make_move('+8822UM')
    board.make_move('-3122GI')
    moves = board.genmove()
    for move in moves:
        print(move)
        code = board.encode_move(move)
        print(code)
        assert 0 < code < 2**12


def test_illegal_move():
    board = miniosl.State()
    move = board.to_move('+7775FU')
    assert not move.is_normal()

    move = board.to_move('7g7f')
    assert move.is_normal()

    move = board.to_move('7g7c')
    assert not move.is_normal()


def test_illegal_make_move():
    board = miniosl.State()
    move = board.to_move('+7776FU')
    assert move.is_normal()
    board.make_move(move)

    # turn mismatch
    with pytest.raises(ValueError):
        board.make_move(move)

    board.make_move('-3334FU')

    # piece mismatch
    with pytest.raises(ValueError):
        board.make_move(move)
