import miniosl
import minioslcc
import numpy as np
import copy
import pytest

sfen = 'sfen lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/P8/1PPPPPPPP/1B5R1/LNSGKGSNL b - 1'


def test_initial_state():
    miniosl.UI.default_prefer_text = True
    ui = miniosl.UI()
    state = ui._state
    assert isinstance(state, miniosl.State)
    assert isinstance(state, minioslcc.State)
    assert isinstance(state, minioslcc.BaseState)
    assert ui.count_hand(miniosl.black, miniosl.pawn) == 0
    assert ui.count_hand(miniosl.white, miniosl.pawn) == 0
    assert ui.turn() == miniosl.black
    b2 = miniosl.UI(sfen)
    assert b2


def test_make_move():
    board = miniosl.UI()
    board.make_move('7g7f')
    copied = miniosl.UI(board)
    copied2 = copy.deepcopy(board)
    assert board.turn() == miniosl.white
    assert board.hash_code() == copied.hash_code()
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
    board = miniosl.UI()
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
    board.unmake_move()
    moves = board.genmove()
    assert '7g7f' in [m.to_usi() for m in moves]
    moves_ja = board.genmove_ja()
    assert len(moves) == len(moves_ja)


def test_to_state_feature():
    board = miniosl.UI()
    feature = board.to_np_state_feature()
    assert isinstance(feature, np.ndarray)
    board_channels = miniosl.input_unit//81 \
        - miniosl.channels_per_history*miniosl.history_length
    assert feature.shape == (board_channels, 9, 9)

    board = miniosl.UI("sfen l5SSl/3g5/3sp+P+Np1/p5g1p/1n1pB1k2/P2PP3P/B+r2gGN2/9/L3K2RL b 5Psn3p 1")
    assert board.count_hand(miniosl.black, miniosl.pawn) == 5
    assert board.count_hand(miniosl.white, miniosl.pawn) == 3
    feature = board.to_np_state_feature()
    tbl = miniosl.channel_id
    assert pytest.approx(feature[tbl['black-hand-pawn']][0][0]) == 5 / 18
    assert pytest.approx(feature[tbl['white-hand-pawn']][0][0]) == 3 / 18
    assert pytest.approx(feature[tbl['white-hand-silver']][0][0]) == 1 / 4
    assert pytest.approx(feature[tbl['white-hand-knight']][0][0]) == 1 / 4
    assert pytest.approx(feature[tbl['white-hand-lance']][0][0]) == 0


def test_to_np_cover():
    board = miniosl.UI()
    feature = board.to_np_cover()
    assert isinstance(feature, np.ndarray)
    assert feature.shape == (2, 9, 9)


def test_board_csa():
    board = miniosl.UI()
    csa = board.to_csa()
    for r, line in enumerate(csa.split('\n')):
        if r < 9:
            assert len(line) == 2+3*9
            assert line[0:2] == 'P'+str(r+1)
        else:
            assert line == '+' or line == '-' or len(line) < 1


def test_replay():
    board = miniosl.UI()
    board.make_move('+7776FU')
    board.make_move('-3334FU')


def test_illegal_move():
    board = miniosl.UI()
    with pytest.raises(ValueError):
        move = board.to_move('+7775FU')
        assert not move.is_normal()

    move = board.to_move('7g7f')
    assert move.is_normal()

    with pytest.raises(ValueError):
        move = board.to_move('7g7c')
        assert not move.is_normal()


def test_illegal_make_move():
    board = miniosl.UI()
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


def test_hash_code():
    board = miniosl.UI()
    a, b = board.hash_code()
    assert a != 0
    assert b == 0


def test_drawing(tmp_path):
    board = miniosl.UI()
    board.to_img()
    anim = board.to_anim()
    anim.save(f'{tmp_path}/test.gif', writer='PillowWriter')


def test_usi_history():
    shogi = miniosl.UI()
    usi = shogi.to_usi_history()
    assert usi == 'startpos' or usi == 'startpos moves'
    shogi.make_move('+2726FU')
    usi = shogi.to_usi_history()
    usi == 'startpos moves 2g2f'
    shogi.make_move('-8384FU')
    usi = shogi.to_usi_history()
    usi == 'startpos moves 2g2f 8c8d'


def test_repeat():
    s = miniosl.UI('startpos moves 3i4h 7a6b 4h3i 6b7a 3i4h 7a6b 4h3i 6b7a')
    s.first()
    assert s.repeat_count() == 2   # including future ...
    for i in range(1, len(s)):
        s.go(1)
        assert s.repeat_count() == i // 4

    moves = '3i4h 7a6b 4h3i 6b7a 3i4h 7a6b 4h3i 6b7a'.split()
    s = miniosl.UI()
    assert s.repeat_count() == 0
    for i, m in enumerate(moves, 1):
        s.make_move(m)
        assert s.repeat_count() == i // 4


def test_read_japanese():
    board = miniosl.UI()
    assert board.read_japanese_move('☗７六歩') == board.to_move('7g7f')
    assert board.read_japanese_move('☗５八金右') == board.to_move('4i5h')


def test_show_features():
    board = miniosl.UI()
    board.show_features('black-pawn')
    board.show_features('black-hand-pawn')
    for i in range(54):
        board.show_features(i)

    board = miniosl.UI('sfen 7nk/7n1/9/7N1/9/9/1p7/9/1K7 b LP2r2b4g4sn3l16p 1')
    for i in range(54):
        print(i)
        board.show_features(i)


def test_analyze():
    shogi = miniosl.UI()
    if miniosl.has_pretrained_eval():
        shogi.load_eval()
        shogi.analyze()


def test_checkmate():
    mate1_sfen = 'sfen k8/9/P8/9/9/9/9/+P+P7/K8 b G2r2b3g4s4n4l15p 1'
    shogi = miniosl.UI()
    shogi.load_record(mate1_sfen)
    assert not shogi.in_check()
    assert not shogi.in_checkmate()
    moves = shogi.genmove_check()
    assert len(moves) == 4
    csa_moves = [_.to_csa() for _ in moves]
    assert '+9392TO' in csa_moves
    assert '+0092KI' in csa_moves
    shogi.make_move('+9392TO')
    assert shogi.in_check()
    assert not shogi.in_checkmate()
    shogi.unmake_move()
    shogi.make_move('+0092KI')
    assert shogi.in_check()
    assert shogi.in_checkmate()
    shogi.unmake_move()
