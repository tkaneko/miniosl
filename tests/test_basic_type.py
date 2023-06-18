import miniosl


def test_color():
    assert miniosl.black != miniosl.white
    assert miniosl.alt(miniosl.black) == miniosl.white
    assert miniosl.black == miniosl.alt(miniosl.white)
    assert miniosl.to_csa(miniosl.black) == '+'
    assert miniosl.to_csa(miniosl.white) == '-'


def test_ptype():
    assert miniosl.king != miniosl.pawn
    assert miniosl.to_csa(miniosl.king) == 'OU'
    assert miniosl.to_csa(miniosl.pawn) == 'FU'
    assert int(miniosl.empty) == 0
    assert int(miniosl.edge) == 1


def test_square():
    s51 = miniosl.Square(5, 1)
    s59 = miniosl.Square(5, 9)
    assert s59 != s51
    assert s59 == miniosl.Square(5, 9)
    assert s51.to_usi() == '5a'
    assert s51.to_csa() == '51'

    for x in range(1, 10):
        for y in range(1, 10):
            s = miniosl.Square(x, y)
            assert s.is_onboard()
            assert not s.is_piece_stand()


def test_piece():
    base = miniosl.State()
    s59 = miniosl.Square(5, 9)
    piece = base.piece_at(s59)
    assert piece.ptype() == miniosl.king
    assert piece.color() == miniosl.black
    assert piece.square() == s59
    id = piece.id()
    assert base.piece(id) == piece
    assert piece == base.piece_at(s59)
    assert piece != base.piece_at(miniosl.Square(5, 1))


def test_move():
    board = miniosl.State()
    m7776 = board.to_move('+7776FU')
    assert m7776 == board.to_move('+7776FU')

    assert m7776.to_csa() == '+7776FU'
    assert m7776.to_usi() == '7g7f'
    assert m7776.ptype() == miniosl.pawn
    assert m7776.old_ptype() == miniosl.pawn
    assert m7776.src() == miniosl.Square(7, 7)
    assert m7776.dst() == miniosl.Square(7, 6)
    assert m7776.is_normal()
    assert not m7776.is_capture()
    assert not m7776.is_promotion()
    assert not m7776.is_drop()
    assert m7776.color() == miniosl.black

    board.make_move(m7776)

    m3334 = board.to_move('-3334FU')
    assert m7776 != m3334

    assert m3334.to_csa() == '-3334FU'
    assert m3334.to_usi() == '3c3d'
    assert m3334.ptype() == miniosl.pawn
    assert m3334.old_ptype() == miniosl.pawn
    assert m3334.src() == miniosl.Square(3, 3)
    assert m3334.dst() == miniosl.Square(3, 4)
    assert m3334.is_normal()
    assert not m3334.is_capture()
    assert not m3334.is_promotion()
    assert not m3334.is_drop()
    assert m3334.color() == miniosl.white

    board.make_move(m3334)

    m8822 = board.to_move('+8822UM')
    assert m8822 != m7776
    assert m8822 != m3334

    assert m8822.to_csa() == '+8822UM'
    assert m8822.to_usi() == '8h2b+'
    assert m8822.ptype() == miniosl.pbishop
    assert m8822.old_ptype() == miniosl.bishop
    assert m8822.src() == miniosl.Square(8, 8)
    assert m8822.dst() == miniosl.Square(2, 2)
    assert m8822.is_normal()
    assert m8822.is_capture()
    assert m8822.capture_ptype() == miniosl.bishop
    assert m8822.is_promotion()
    assert not m8822.is_drop()
    assert m8822.color() == miniosl.black

    board.make_move(m8822)

    m3122 = board.to_move('-3122GI')
    assert m3122.to_csa() == '-3122GI'
    assert m3122.to_usi() == '3a2b'
    assert m3122.ptype() == miniosl.silver
    assert m3122.old_ptype() == miniosl.silver
    assert m3122.src() == miniosl.Square(3, 1)
    assert m3122.dst() == miniosl.Square(2, 2)
    assert m3122.is_normal()
    assert m3122.is_capture()
    assert m3122.capture_ptype() == miniosl.pbishop
    assert not m3122.is_promotion()
    assert not m3122.is_drop()
    assert m3122.color() == miniosl.white

    board.make_move(m3122)

    m0045 = board.to_move('+0045KA')
    assert m0045.to_csa() == '+0045KA'
    assert m0045.to_usi() == 'B*4e'
    assert m0045.ptype() == miniosl.bishop
    assert m0045.old_ptype() == miniosl.bishop
    assert m0045.src().is_piece_stand()
    assert m0045.dst() == miniosl.Square(4, 5)
    assert m0045.is_normal()
    assert not m0045.is_capture()
    assert not m0045.is_promotion()
    assert m0045.is_drop()
    assert m0045.color() == miniosl.black


def test_bitset():
    mobility = miniosl.ptype_move_direction[miniosl.knight]
    one_hot = 2**int(miniosl.UUR)
    assert (mobility & one_hot) != 0
