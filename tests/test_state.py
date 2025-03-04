import miniosl
import minioslcc
import numpy as np
import copy
import pytest

sfen = (
    'sfen lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/P8/1PPPPPPPP/1B5R1/LNSGKGSNL b - 1'
)


def test_state():
    base = minioslcc.State()
    assert isinstance(base, miniosl.State)
    assert isinstance(base, miniosl.BaseState)
    assert base.count_hand(miniosl.black, miniosl.pawn) == 0
    assert base.count_hand(miniosl.white, miniosl.pawn) == 0
    assert base.turn == miniosl.black

    board = miniosl.State()
    assert isinstance(board, miniosl.State)
    assert isinstance(board, minioslcc.State)
    assert isinstance(board, minioslcc.BaseState)
    assert base.count_hand(miniosl.black, miniosl.pawn) == 0
    assert base.count_hand(miniosl.white, miniosl.pawn) == 0
    assert base.turn == miniosl.black
    b2 = miniosl.usi_board(sfen)
    assert b2


def test_make_move():
    board = miniosl.State()
    board.make_move('7g7f')
    copied = miniosl.State(board)
    copied2 = copy.copy(board)
    copied3 = copy.deepcopy(board)
    assert copied2 == copied3
    assert board.turn == miniosl.white
    assert board.to_usi() == copied.to_usi()
    assert board.to_usi() == copied2.to_usi()
    assert board.to_csa() == copied.to_csa()
    board.make_move('-3334FU')
    assert board.turn == miniosl.black
    assert board.to_usi() != copied.to_usi()
    assert board.to_usi() != copied2.to_usi()
    assert copied.turn == miniosl.white
    assert copied2.turn == miniosl.white
    assert copied2 == copied3
    board.make_move('+8822UM')
    assert board.turn == miniosl.white
    assert board.count_hand(miniosl.black, miniosl.pawn) == 0
    assert board.count_hand(miniosl.black, miniosl.bishop) == 1


def test_genmove():
    board = miniosl.State()
    moves = board.genmove()
    assert '7g7f' in [m.to_usi() for m in moves]
    assert '+7776FU' in [m.to_csa() for m in moves]
    assert len(moves) == 30
    copied = copy.copy(board)
    board.make_move('+7776FU')
    moves = board.genmove()
    assert '+7776FU' not in [m.to_csa() for m in moves]
    moves = copied.genmove()
    assert '+7776FU' in [m.to_csa() for m in moves]


def test_to_np_cover():
    board = miniosl.State()
    feature = board.to_np_cover()
    assert isinstance(feature, np.ndarray)
    assert feature.shape == (2, 9, 9)


def test_np_state_feature():
    board = miniosl.State()
    data = board.to_np_state_feature()
    assert isinstance(data, np.ndarray)
    assert data.dtype == np.float32


def test_board_csa():
    board = minioslcc.State()
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


def test_encode_move():
    board = miniosl.State()
    board.make_move('+7776FU')
    board.make_move('-3334FU')
    board.make_move('+8822UM')
    board.make_move('-3122GI')
    moves = board.genmove()
    for move in moves:
        code = board.encode_move(move)
        assert 0 < code < 2**12


def test_illegal_move():
    board = miniosl.State()
    with pytest.raises(ValueError):
        move = board.to_move('+7775FU')
        assert not move.is_normal()

    move = board.to_move('7g7f')
    assert move.is_normal()

    with pytest.raises(ValueError):
        move = board.to_move('7g7c')
        assert not move.is_normal()

    board.make_move('+7776FU')

    # turn mismatch
    with pytest.raises(ValueError):
        board.make_move(move)

    board.make_move('-3334FU')

    # piece mismatch
    with pytest.raises(ValueError):
        board.make_move(move)


def test_hash_code():
    board = miniosl.State()
    a, b = board.hash_code()
    assert a != 0
    assert b == 0


def test_read_japanese():
    board = miniosl.State()
    assert board.read_japanese_move('▲７六歩') == board.to_move('7g7f')
    assert board.read_japanese_move('☗５八金右') == board.to_move('4i5h')
    assert board.read_japanese_move('☗５八金左') == board.to_move('6i5h')


def test_policy_move_label():
    board = miniosl.State()
    moves = board.genmove()
    for move in moves:
        code = move.policy_move_label
        assert board.decode_move_label(code) == move


def test_816k():
    board = miniosl.State()
    assert board.shogi816k_id() == miniosl.hirate_816k_id
    board.make_move('+7776FU')
    assert board.shogi816k_id() is None

    id = 4081
    state = miniosl.shogi816k(id)
    assert id == state.shogi816k_id()

    v, i = state.guess_variant()
    assert v == miniosl.Shogi816K
    assert i == id


aozora_csa = \
"""P1-KY-KE-GI-KI-OU-KI-GI-KE-KY
P2 * -HI *  *  *  *  * -KA * 
P3 *  *  *  *  *  *  *  *  * 
P4 *  *  *  *  *  *  *  *  * 
P5 *  *  *  *  *  *  *  *  * 
P6 *  *  *  *  *  *  *  *  * 
P7 *  *  *  *  *  *  *  *  * 
P8 * +KA *  *  *  *  * +HI * 
P9+KY+KE+GI+KI+OU+KI+GI+KE+KY
+
"""


def test_aozora():
    board = miniosl.aozora()
    csa_str = board.to_csa()
    assert csa_str == aozora_csa

    assert board.shogi816k_id() is None
    board.make_move('+1911NY')
    assert board.shogi816k_id() is None

    v, id = board.guess_variant()
    assert v == miniosl.Aozora


def test_checkmate1ply():
    # both kings
    state = miniosl.usi_state('sfen 8k/9/8P/9/9/9/9/7+P+P/7+PK b G2r2b3g4s4n4l14p 1')
    assert state.king_active(miniosl.black)
    assert state.king_active(miniosl.white)
    assert state.turn == miniosl.black
    assert state.try_checkmate_1ply().to_csa() == '+0012KI'

    # single king
    # # not ready
    # state = miniosl.usi_state('sfen 8k/7g1/8P/9/9/9/9/9/9 b GN2r2b2g4s3n4l17p 1')
    # assert state.king_active(miniosl.white)
    # assert state.turn == miniosl.black
    # assert not state.in_check()
    # assert state.try_checkmate_1ply().to_csa() == '+0012KI'
