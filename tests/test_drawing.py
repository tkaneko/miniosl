import miniosl
import numpy as np


def test_draw_basestate():
    r = miniosl.usi_record('startpos moves 7g7f')
    array = r.export_all()
    assert len(array) == len(r)
    assert len(array[0]) == 4
    t = miniosl.to_state_label_tuple(array[0])
    assert isinstance(t.state, miniosl.BaseState)
    assert t.state.to_png()
    assert t.state.to_png(id=3)
    assert t.state.to_png(plane=np.zeros((9, 9)))


def test_draw_state():
    board = miniosl.State()
    svg = board.to_svg()
    assert svg
    png = board.to_png()
    assert png


def test_draw_after_make_move():
    board = miniosl.State()
    board.make_move('7g7f')
    assert board.last_to().to_xy() == (7, 6)
    svg = board.to_svg()
    assert svg
    board.make_move('-3334FU')
    assert board.last_to().to_xy() == (3, 4)
    svg = board.to_svg()
    svg = board.to_svg(decorate=True)
    assert svg
    png = board.to_png(decorate=True)
    assert png
