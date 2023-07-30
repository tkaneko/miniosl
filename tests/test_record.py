import miniosl
import minioslcc
import numpy as np
import copy
import random

sfen = 'startpos moves 2g2f 8c8d 2f2e 8d8e 9g9f 4a3b 3i3h 7a7b 6i7h 5a5b 4g4f'\
    + ' 9c9d 3h4g 7c7d 1g1f 7d7e 2e2d 2c2d 2h2d 8e8f 8g8f P*2c 2d7d 8b8f 5i5h'\
    + ' 7b7c 7d7e 2c2d P*8g 8f8b 7e3e 3b2c 3e3f 2d2e 7g7f 5b4b 1f1e 3c3d 4i3h'\
    + ' 7c6d 8h2b+ 3a2b 4f4e 2b3c 4g5f 8a7c 6g6f P*8f 8g8f 8b8f P*8g 8f8a' \
    + ' 3f4f 6a6b 7i6h 5c5d 6h6g 4b3b 3g3f 5d5e 5f4g 6d5c 2i3g 6c6d 7f7e 8a8d'\
    + ' 5h4h B*2d 3f3e 2d3e 4f3f P*8h 7h8h 1c1d 1e1d 1a1d 1i1d 2c1d P*2b 3c2b'\
    + ' 4e4d 4c4d 8h7h 6d6e P*1b 1d2d L*7d P*7b 1b1a+ 2b1a 8i7g 4d4e 7d7c+' \
    + ' 7b7c 7g6e L*4f 6e5c 4f4g 4h5h 3e1g+ 3g4e 2d3e P*4d 8d4d P*3c 2a3c' \
    + ' B*2a 3b2a 4e3c+ S*3b S*2d 4g4h+ 5h6h 4h5h 6g5h P*6g 7h6g B*1e 2d1e' \
    + ' 3b3c 3f3g 1g1f L*1g N*4f 1g1f 4f5h+ 6h5h 6b5c B*6b 4d4c N*2d S*2c' \
    + ' P*4d 5c4d B*5b 1a2b 5b4c+ 4d4c R*4a B*3a 4a4c+ 3c2d 1e2d N*4f 5h4g' \
    + ' 3e3f 3g3f N*3e 2d3e L*1d S*3b 2a1a G*2a 1a1b 4c2c 2b2c N*2d 1b1c' \
    + ' 3b2c+ 1c2c 3e3d 2c2d G*2c'


def test_copy():
    r = miniosl.usi_record('startpos')
    c = copy.copy(r)
    assert r == c
    move = r.initial_state.to_move('7g7f')
    r.append_move(move, False)
    assert r != c


def test_anim():
    r = miniosl.usi_record(sfen)
    assert len(r) > 10
    assert r.to_usi() != ''
    s = r.replay(15)
    assert isinstance(s, miniosl.State)
    anim = r.to_apng(10)
    assert anim


def test_np():
    r = miniosl.usi_record(sfen)
    array = r.export_all()
    assert len(array) == len(r)
    assert len(array[3]) == 4
    t = miniosl.to_state_label_tuple256(array[3])
    assert isinstance(t.state, minioslcc.BaseState)
    assert isinstance(t.move, minioslcc.Move)
    assert isinstance(t.result, minioslcc.GameResult)
    s = miniosl.State(t.state)
    assert s.to_usi() == t.state.to_usi()

    nparray = np.array(array, dtype=np.uint64)
    np.random.shuffle(nparray)
    assert len(nparray[3]) == 4
    t = miniosl.to_state_label_tuple256(nparray[3].tolist())
    assert isinstance(t.state, minioslcc.BaseState)
    assert isinstance(t.move, minioslcc.Move)
    assert isinstance(t.result, minioslcc.GameResult)
    code = t.state.to_np_pack()
    assert code.shape == (4,)

    s = miniosl.State(t.state)
    assert s.to_usi() == t.state.to_usi()


def test_channel_name():
    assert miniosl.channel_id['black-pawn'] == int(miniosl.pawn)+14


def test_gamemanager():
    mgr = miniosl.GameManager()
    m7776 = mgr.state.to_move("+7776FU")
    ret = mgr.make_move(m7776)
    assert ret == miniosl.InGame
    m3334 = mgr.state.to_move("-3334FU")
    ret = mgr.make_move(m3334)
    assert ret == miniosl.InGame

    moves = ["+3948GI", "-7162GI", "+4839GI", "-6271GI", "+3948GI", "-7162GI",
             "+4839GI", "-6271GI", "+3948GI", "-7162GI", "+4839GI"]
    for move in moves:
        ret = mgr.make_move(mgr.state.to_move(move))
        assert ret == miniosl.InGame

    move = mgr.state.to_move("-6271GI")
    ret = mgr.make_move(move)
    assert ret == miniosl.Draw


def test_gamemanager_feature():
    mgr = miniosl.GameManager()
    feature = mgr.export_heuristic_feature()
    print(feature.shape)


def test_parallelgamemanager():
    N = 4
    N_GAMES = 10
    mgrs = miniosl.ParallelGameManager(N, True)
    feature = mgrs.export_heuristic_feature_parallel()
    print(feature.shape)
    cnt = 0
    while len(mgrs.completed_games) < N_GAMES:
        moves_chosen = []
        for g in range(N):
            moves = mgrs.games[g].state.genmove()
            moves_chosen.append(random.choice(moves))
        mgrs.make_move_parallel(moves_chosen)
        cnt += 1
        assert cnt < N_GAMES*miniosl.draw_limit


def test_to_np_feature_labels():
    item = miniosl.StateRecord320.test_item()
    input, move_label, value_label, aux_label = item.to_np_feature_labels()
    assert input.shape == (len(miniosl.channel_id), 9, 9)
    assert aux_label.shape == (12, 9, 9)


def test_end_by_rule():
    # repeat with check
    sfen = 'startpos moves 2g2f 8c8d 6i7h 8d8e 2f2e 4a3b 3i3h 5a5b 1g1f 7a7b 1f1e 8e8f 8g8f 8b8f P*8g 8f8d 5i5h 7c7d 2e2d 2c2d 2h2d 8a7c P*2c 8d8e 2c2b+ 3a2b 2d7d 8e2e B*8b 2e2h+ 7d7c+ 7b7c 8b7c+ P*8f 7c9a 8f8g+ 7h8g R*8d P*8f 8d2d P*2e 2d2e 9a7c 2h1i L*5e P*7h 7i7h L*5d 5e5d 5c5d 7c8c P*7b N*7d 2e2i+ 3h2i 1i2i S*3h 2i2h 8c8d S*7c 8d9e 7c7d L*7f N*8c 7f7d 8c9e P*2i 2h1h R*8a 9e8g+ 8a6a+ 5b6a S*5c G*5a G*5b 5a5b 5c5b+ 6a5b 7h8g R*6i 5h6i N*7e 8g7f B*7h 6i7h P*8g 8h7i S*8h R*8a 8h7i 7h7i 7e6g+ 7f6g G*8h 7i6h B*7i 6h5h 8h8i B*6a 5b4b G*5b 4b3a N*2d 1h1e 2d3b+ 3a3b 5b4b 3b2c 6a4c+ L*6d P*6f N*5e N*2g 1e2f S*3e 5e6g+ 5h5i 7i5g+ G*2d 2f2d 3e2d 2c2d R*2e 2d1d 2e1e 1d2d 1e2e 2d1d 2e1e 1d2d 1e2e 2d1d 2e1e 1d2d 1e2e'
    moves = sfen.split()[2:]
    mgr = miniosl.GameManager()
    for i, move in enumerate(moves):
        ok = mgr.make_move(mgr.state.to_move(move))
        assert ok == (miniosl.InGame if i+1 < len(moves) else miniosl.WhiteWin)
    assert mgr.record.result == miniosl.WhiteWin
    text = mgr.record.to_usi()
    r2 = miniosl.usi_record(text)
    assert r2.result == miniosl.WhiteWin
    r3 = miniosl.usi_sub_record(text)
    assert r3.result == miniosl.WhiteWin


def test_end_by_rule2():
    # no legal moves
    sfen = 'startpos moves 7g7f 8c8d 9g9f 8d8e 8h7g 3c3d 2g2f 2b7g+ 8i7g 3a2b 3i4h 2b3c 3g3f 9c9d 5i6h 8e8f 8g8f 8b8f 7g6e 8f8i+ 6e5c+ 4a3b 2i3g 8i9i 5c4c 3b4c 3g2e 3c4b 4h3g B*5d 4g4f 4b3c B*5e 3c2d 5e1a+ 5d7f P*7h P*8h 7i8h P*5f 5g5f 2d2e 8h9i L*8g L*6f 5a5b 1a2a N*5e 5f5e 6c6d N*8h 8g8h+ 9i8h 2e3f 3g3f 4c4b 2a7f P*8g 8h8g 5b5c R*3c 5c6b B*3e 3d3e 3f3e N*5d 5e5d 6b7b 3c3a+ 4b3b P*3c 3b3c N*7d 7c7d 5d5c+ 6d6e 6f6e B*5e 6e6a+ 7a8b 6a6b 7b7c 6h5g N*4e 5g5f 4e5g+ 5f5g P*6f 6g6f 5e6d 7f8e N*9c 8e8d 7c8d N*7f 8d8e 8g8f 8e8f P*8g 8f8g P*8h 8g8h G*6g 6d5e 7f8d P*5f 6g5f 5e6d L*7i 8h8i 5c4b P*4d 3a1a 6d4b 1a5a 7d7e 5g5h 3c2d 3e2d 2c2d 4i3h 8i9i 5f6e S*5f N*5e P*3f S*4g 5f5g+ 5h5g 3f3g+ 3h3g 4d4e 5a4b 8a7c 6e7d 4e4f 4g4f S*4i B*5b P*6c 5b6c+ B*6e 6f6e 7c6e 5g6f 7e7f 6f7f P*8h G*8g 8h8i+ S*6f 6e5g+ 4f5g 9d9e 9f9e 9c8e 7f8e 8b7c 7d7c 1c1d 8e7d 2d2e 2f2e 9a9e P*9f 9e9f 8g9f P*3f 3g3f 8i7i P*6h 7i8i L*7g 1d1e 7d7e L*6d 6c6d 8i7i 9f9g 7i7h 6i7h 9i8i P*8e 8i9i 4b4a P*9d B*6a 9d9e 7e8f 9e9f 9g9f P*9c S*8b 9c9d 7g7f 9i8i L*3d 9d9e 7h7i 8i9i 9f9e 9i9h 3d3a+ 9h9i 6a4c+ 9i9h 6b5b 4i5h+ 9e9d 5h6h 5g6h P*8g 8f7e 9h9i P*3d 8g8h+ 7i8h 9i8h 6d6e 8h7h 6h7g 7h8i N*5d G*7i 7g8h 7i7h 8h8g 7h8h P*7g 8h7i 9d9c 7i6i 7e8f 8i7i 8f7e 6i6h P*6c 6h7h 8g7h 7i8h 7h8i 8h8i 7c8c 8i9i S*6h S*5h 2h5h 1e1f 1g1f 9i9h 7e6d 9h8h G*7i 8h9i 5h5g 9i9h 4a4b 9h9i 7f7e'
    moves = sfen.split()[2:]
    mgr = miniosl.GameManager()
    for i, move in enumerate(moves):
        ok = mgr.make_move(mgr.state.to_move(move))
        assert ok == (miniosl.InGame if i+1 < len(moves) else miniosl.BlackWin)
    assert mgr.record.result == miniosl.BlackWin
    text = mgr.record.to_usi()
    r2 = miniosl.usi_record(text)
    assert r2.result == miniosl.BlackWin
    r3 = miniosl.usi_sub_record(text)
    assert r3.result == miniosl.BlackWin


def test_end_by_rule3():
    # 3rd repetition
    sfen = 'startpos moves 7g7f 3c3d 8h2b+ 3a2b 6g6f 2b3c 1g1f 7a7b 3g3f 1c1d 7i6h 6c6d 5g5f 4a3b 6h7g 8c8d 2g2f 9c9d 3i4h 7b6c 4h3g 6c5d 7f7e 8d8e 6i7h 9d9e 4i5h 6d6e 6f6e 5d6e P*6f 6e5d 5h4h P*6e 6f6e 8b8d 7h6g 8d7d 7e7d 7c7d P*7c P*6f 6g5g 5d6e 7g6h B*8h 5f5e 8h9i+ 8i7g 8a7c R*8b 3c4d B*6c 6f6g+ 6h6g 4d5e 7g6e 7c6e P*7f N*7a S*5d L*5b 5d6e 7a6c N*6d 3b3c 6g5h B*3i 4h4i P*6f 5g4f 3i2h+ 3g2h 1d1e 2h3g R*8i 5i6h 6f6g+ 6h6g 5e6f 6g7h 6f7g 7h6g 7g6f 6g7h 6f7g 7h6g 5c5d 6d5b+ 6a5b P*6b 5a4a 6b6a+ N*5a 6a5a 4a5a N*7c 7g6f 6g7h 6f7g 7h6g 7g6f 6g7h 6f7g 7h6g 7g6f'
    r0 = miniosl.usi_record(sfen)
    assert r0.result == miniosl.InGame
    moves = sfen.split()[2:]
    assert moves[0] == '7g7f'
    mgr = miniosl.GameManager()
    for i, move in enumerate(moves):
        ok = mgr.make_move(mgr.state.to_move(move))
        cnt = mgr.record.repeat_count()
        if cnt:
            prev = mgr.record.previous_repeat_index()
            print(i+1, cnt, prev, mgr.record.repeat_count(prev))
        if ok != miniosl.InGame:
            print(i+1, '/', len(moves), move)
        assert ok == miniosl.InGame


def test_end_by_rule4():
    # 3rd repetition
    sfen = 'startpos moves 7g7f 3c3d 8h2b+ 3a2b 6g6f 2b3c 1g1f 7a7b 3g3f 1c1d 7i6h 6c6d 5g5f 4a3b 6h7g 8c8d 2g2f 9c9d 3i4h 7b6c 4h3g 6c5d 7f7e 8d8e 6i7h 9d9e 4i5h 6d6e 6f6e 5d6e P*6f 6e5d 5h4h P*6e 6f6e 8b8d 7h6g 8d7d 7e7d 7c7d P*7c P*6f 6g5g 5d6e 7g6h B*8h 5f5e 8h9i+ 8i7g 8a7c R*8b 3c4d B*6c 6f6g+ 6h6g 4d5e 7g6e 7c6e P*7f N*7a S*5d L*5b 5d6e 7a6c N*6d 3b3c 6g5h B*3i 4h4i P*6f 5g4f 3i2h+ 3g2h 1d1e 2h3g R*8i 5i6h 6f6g+ 6h6g 5e6f 6g7h 6f7g 7h6g 7g6f 6g7h 6f7g 7h6g'
    mgr = miniosl.GameManager()
    for i, move in enumerate(sfen.split()[2:]):
        ok = mgr.make_move(mgr.state.to_move(move))
        assert ok == miniosl.InGame
    ok = mgr.make_move(mgr.state.to_move('5c5d'))
    assert ok == miniosl.InGame
    assert mgr.record.repeat_count() == 0
