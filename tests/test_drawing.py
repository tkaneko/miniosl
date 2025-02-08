import miniosl
import numpy as np
import matplotlib
matplotlib.use('Agg')


def test_draw_basestate():
    r = miniosl.usi_record('startpos moves 7g7f')
    t = r.initial_state
    assert isinstance(t, miniosl.BaseState)
    assert t.to_img()
    assert t.to_img(id=3)
    assert t.to_img(plane=np.zeros((9, 9)))


def test_draw_state():
    board = miniosl.State()
    img = board.to_img()
    assert img


def test_draw_after_make_move():
    board = miniosl.State()
    m7g7f = board.to_move('7g7f')
    board.make_move(m7g7f)
    img = board.to_img(last_to=m7g7f.dst)
    assert img
    board.make_move('-3334FU')
    img = board.to_img(last_to=miniosl.Square(3, 4))
    img = board.to_img(decorate=True)
    assert img
