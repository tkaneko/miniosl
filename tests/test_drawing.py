import miniosl
import numpy as np


def test_draw_basestate():
    r = miniosl.usi_record('startpos moves 7g7f')
    t = r.initial_state
    assert isinstance(t, miniosl.BaseState)
    assert t.to_png()
    assert t.to_png(id=3)
    assert t.to_png(plane=np.zeros((9, 9)))


def test_draw_state():
    board = miniosl.State()
    svg = board.to_svg()
    assert svg
    png = board.to_png()
    assert png


def test_draw_after_make_move():
    board = miniosl.State()
    m7g7f = board.to_move('7g7f')
    board.make_move(m7g7f)
    svg = board.to_svg(last_to=m7g7f.dst())
    assert svg
    board.make_move('-3334FU')
    svg = board.to_svg(last_to=miniosl.Square(3, 4))
    svg = board.to_svg(decorate=True)
    assert svg
    png = board.to_png(decorate=True)
    assert png
