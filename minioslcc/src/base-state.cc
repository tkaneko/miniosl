#include "base-state.h"
#include "record.h"
#include "impl/bitpack.h"
#include "impl/rng.h"
#include <iostream>

osl::BaseState::BaseState() {
  initEmpty();
}

osl::BaseState::BaseState(GameVariant v, std::optional<int> additional_param) {
  init(v, additional_param);
}

void osl::BaseState::initFinalize(){
  for (Ptype ptype: piece_stand_order) {
    stand_count[BLACK][basic_idx(ptype)] = countPiecesOnStandBit(BLACK, ptype);
    stand_count[WHITE][basic_idx(ptype)] = countPiecesOnStandBit(WHITE, ptype);
  }

  pawnMask[0] = XNone;
  pawnMask[1] = XNone;
  for (int num: to_range(PAWN)) {
    if (! active_set.test(num))
      continue;
    Piece p=pieceOf(num);
    Player player=p.owner();
    Square pos=p.square();
    if(!pos.isPieceStand() && !p.isPromoted()){
      if (pawnInFile(player,pos.x())) {
	throw csa::ParseError("2FU!");
      }
      set_x(pawnMask[idx(player)], pos);
    }
  }
  assert(check_internal_consistency());
}

void osl::BaseState::initEmpty() {
  side_to_move=BLACK;
  for (int ipos=0;ipos<Square::SIZE;ipos++) {
    setBoard(Square::nth(ipos),Piece::EDGE());
  }
  for (int y: board_y_range())
    for (int x: board_x_range()) {
      setBoard(Square(x,y),Piece::EMPTY());
    }
  //  promoteMask.clearAll();
  stand_mask[BLACK].resetAll();
  stand_mask[WHITE].resetAll();
  stand_count[BLACK].fill(0);
  stand_count[WHITE].fill(0);
  active_set.resetAll();
  pawnMask[0] = XNone;
  pawnMask[1] = XNone;
  for (int num: all_piece_id()){
    pieces[num]=Piece(WHITE,piece_id_ptype[num],num,Square::STAND());
  }
}
  
void osl::BaseState::initAozora() {
  const std::tuple<Player, Square, Ptype> pieces[] = {
    {BLACK,Square(1,9),LANCE},  {BLACK,Square(9,9),LANCE},
    {WHITE,Square(1,1),LANCE},  {WHITE,Square(9,1),LANCE},
    {BLACK,Square(2,9),KNIGHT}, {BLACK,Square(8,9),KNIGHT},
    {WHITE,Square(2,1),KNIGHT}, {WHITE,Square(8,1),KNIGHT},
    {BLACK,Square(3,9),SILVER}, {BLACK,Square(7,9),SILVER},
    {WHITE,Square(3,1),SILVER}, {WHITE,Square(7,1),SILVER},
    {BLACK,Square(4,9),GOLD},   {BLACK,Square(6,9),GOLD},
    {WHITE,Square(4,1),GOLD},   {WHITE,Square(6,1),GOLD},
    {BLACK,Square(5,9),KING},   {WHITE,Square(5,1),KING},
    {BLACK,Square(8,8),BISHOP}, {WHITE,Square(2,2),BISHOP},
    {BLACK,Square(2,8),ROOK},   {WHITE,Square(8,2),ROOK}
  };
  for (auto [pl, sq, pt]: pieces)
    setPiece(pl, sq, pt);

  initFinalize();  
}

void osl::BaseState::init816K(int id) {
  if (id < 0)
    id = rngs[0]() % Shogi816K_Size;
  for (int x=9;x>0;x--) {
    setPiece(BLACK, Square(x, 7), PAWN);
    setPiece(WHITE, Square(x, 3), PAWN);
  }
  const int rb = id / 22680;
  assert(0 <= rb && rb < 72);
  int rook = rb / 8, bishop = rb % 8;
  setPiece(BLACK, Square(rook + 1, 8), ROOK);
  setPiece(WHITE, Square(9 - rook, 2), ROOK);
  if (bishop >= rook)
    bishop += 1;
  setPiece(BLACK, Square(bishop + 1, 8), BISHOP);
  setPiece(WHITE, Square(9 - bishop, 2), BISHOP);
  const int kgskl = id % 22680;
  const int king = kgskl / (28 * 15 * 6);
  assert(0 <= king && king < 9);
  setPiece(BLACK, Square(king + 1, 9), KING);
  setPiece(WHITE, Square(9 - king, 1), KING);
  bool filled[10] = { 0 };
  filled[king] = true;
  const int gg = (kgskl % (28 * 15 * 6)) / (15 * 6);
  const int ss = (kgskl % (15 * 6)) / 6;
  const int kk = kgskl % 6;
  auto assign = [&](int nth) {
    int x = 0;
    while (filled[x])
      ++x;
    for (int i=0; i<nth; ++i) {
      ++x;
      while (filled[x])
        ++x;
    }
    filled[x] = true;
    return x;
  };
  for (auto [ptype, id]: {std::make_pair(GOLD, gg), {SILVER, ss}, {KNIGHT, kk}, {LANCE, 0}}) {
    auto [p0, p1] = bitpack::detail::unpack2(id);
    assert(p0 < p1);
    p1 = assign(p1);
    p0 = assign(p0);
    setPiece(BLACK, Square(p0 + 1, 9), ptype);
    setPiece(BLACK, Square(p1 + 1, 9), ptype);
    setPiece(WHITE, Square(9 - p0, 1), ptype);
    setPiece(WHITE, Square(9 - p1, 1), ptype);  
  }
  initFinalize();
}

std::pair<osl::GameVariant, std::optional<int>>
osl::BaseState::guess_variant() const {
  auto opt_id = shogi816kID();
  if (opt_id && opt_id.value() != hirate_816k_id)
    return {Shogi816K, opt_id};
  auto active_count = active_set.countBit();
  if (active_count == 40)
    return {HIRATE, std::nullopt};
  if (active_count == 22
      && active_set.selectBit<PAWN>() == 0)
    return {Aozora, std::nullopt};
  return {UnIdentifiedVariant, std::nullopt};
}

std::optional<int> osl::BaseState::shogi816kID() const {
  int xs[Ptype_SIZE][2] = {{ 0 }};
  for (int x=9;x>0;x--) {
    if (pieceAt(Square(x, 7)).ptype() != PAWN) return std::nullopt;
    if (pieceAt(Square(x, 3)).ptype() != PAWN) return std::nullopt;
    auto p9 = pieceAt(Square(x, 9)), p8 = pieceAt(Square(x, 8));
    if (! p9.isOnBoardByOwner(BLACK) || ! is_basic(p9.ptype())
        || pieceAt(p9.square().rotate180()).ptypeO() != newPtypeO(WHITE, p9.ptype()))
      return std::nullopt;
    if (p8.isOnBoardByOwner(WHITE)
        || (is_piece(p8.ptype())
            && (! is_basic(p8.ptype()) // may be vacant square
                || pieceAt(p8.square().rotate180()).ptypeO() != newPtypeO(WHITE, p8.ptype()))))
      return std::nullopt;
    for (auto [ptype, piece]: {std::make_pair(GOLD, p9), {SILVER, p9}, {KNIGHT, p9}, {LANCE, p9},
                               {KING, p9},
                               {ROOK, p8}, {BISHOP, p8}})
      if (piece.ptype() == ptype) {
        if (xs[idx(ptype)][0])
          xs[idx(ptype)][1] = x;
        else
          xs[idx(ptype)][0] = x;
      }
  }
  int& rook = xs[idx(ROOK)][0], & bishop = xs[idx(BISHOP)][0], & king = xs[idx(KING)][0];
  if (! (rook && bishop && king))
    return std::nullopt;
  if (bishop >= rook)
    bishop -= 1;
  int rb = (rook-1) * 8 + (bishop-1);
  assert(0 <= rb && rb < 72);
  int kgskl = king-1;
  bool placed[10] = { 0 };
  placed[king] = 1;
  int *gg = xs[idx(GOLD)], *ss = xs[idx(SILVER)], *kk = xs[idx(KNIGHT)];
  for (auto [pp, scale]: {std::make_pair(gg, 28), {ss, 15}, {kk, 6}}) {
    assert(placed[pp[0]] == 0);
    placed[pp[0]] = 1;
    pp[0] = std::count(placed+1, placed+pp[0], 0);
    assert(placed[pp[1]] == 0);
    placed[pp[1]] = 1;
    pp[1] = std::count(placed+1, placed+pp[1], 0);
    int code = bitpack::detail::combination_id(pp[1], pp[0]);
    kgskl = kgskl * scale + code;
  }
  return rb * 22680 + kgskl;
}

void osl::BaseState::init(GameVariant v, std::optional<int> additional_param) {
  initEmpty();
  if (v == Shogi816K)
    return init816K(additional_param.value_or(-1));
  if (v == Aozora)
    return initAozora();
  if (v != HIRATE) {
    std::cerr << "unsupported handicap\n";
    throw std::domain_error("unsupported handicap");
  }
  // 歩
  for (int x=9;x>0;x--) {
    setPiece(BLACK,Square(x,7),PAWN);
    setPiece(WHITE,Square(x,3),PAWN);
  }
  // 
  setPiece(BLACK,Square(1,9),LANCE);
  setPiece(BLACK,Square(9,9),LANCE);
  setPiece(WHITE,Square(1,1),LANCE);
  setPiece(WHITE,Square(9,1),LANCE);
  //
  setPiece(BLACK,Square(2,9),KNIGHT);
  setPiece(BLACK,Square(8,9),KNIGHT);
  setPiece(WHITE,Square(2,1),KNIGHT);
  setPiece(WHITE,Square(8,1),KNIGHT);
  //
  setPiece(BLACK,Square(3,9),SILVER);
  setPiece(BLACK,Square(7,9),SILVER);
  setPiece(WHITE,Square(3,1),SILVER);
  setPiece(WHITE,Square(7,1),SILVER);
  //
  setPiece(BLACK,Square(4,9),GOLD);
  setPiece(BLACK,Square(6,9),GOLD);
  setPiece(WHITE,Square(4,1),GOLD);
  setPiece(WHITE,Square(6,1),GOLD);
  //
  setPiece(BLACK,Square(5,9),KING);
  setPiece(WHITE,Square(5,1),KING);
  //
  setPiece(BLACK,Square(8,8),BISHOP);
  setPiece(WHITE,Square(2,2),BISHOP);
  //
  setPiece(BLACK,Square(2,8),ROOK);
  setPiece(WHITE,Square(8,2),ROOK);

  initFinalize();
}
  

osl::BaseState::~BaseState() {
}

void osl::BaseState::setPiece(Player player, Square pos, Ptype ptype) {
  for (int num: all_piece_id()) {
    if (! active_set.test(num)
        && piece_id_ptype[num]==unpromote(ptype)
	&& (ptype!=KING || num==king_piece_id(player))) {
      active_set.set(num);
      Piece p(player,ptype,num,pos);
      pieces[num] = p;
      if (pos.isPieceStand())
	stand_mask[player].set(num);
      else{
	setBoard(pos,p);
	if (ptype==PAWN)
	  set_x(pawnMask[player], pos);
      }
      return;
    }
  }
  std::cerr << "osl::BaseState::setPiece! maybe too many pieces " 
	    << ptype << " " << pos << " " << player << "\n";
  abort();
}

void osl::BaseState::setPieceAll(Player player) {
  for (int num: all_piece_id()) {
    if (! active_set.test(num)) {
      active_set.set(num);
      stand_mask[player].set(num);
      Player pplayer = player;
      /* 片玉しかない問題のため */
      if (num==king_piece_id(alt(player)))
	pplayer=alt(player);
      pieces[num] = Piece(pplayer,piece_id_ptype[num],num,Square::STAND());
    }
  }
}
  
bool osl::BaseState::check_internal_consistency() const {
  // board上の要素のconsistency
  for (int y: board_y_range()) {
    for (int x: board_x_range()) {
      const Square pos(x,y);
      const Piece p0=pieceAt(pos);
      if (p0.isPiece()) {
	if (p0.square()!=pos)
	  return false;
	int num=p0.id();
	if (! is_valid_piece_id(num) || ! active_set.test(num)) 
	  return false;
	Piece p1=pieceOf(num);
	if (p0!=p1) 
	  return false;
      }
    }
  }
  // piecesのconsistency
  for (int num0: all_piece_id()) {
    if (! active_set.test(num0))
      continue;
    if (isOnBoard(num0)) {
      Piece p0=pieceOf(num0);
      Ptype ptype=p0.ptype();
      if (unpromote(ptype)!=piece_id_ptype[num0]) 
	return false;
      if (!p0.isOnBoard()) 
	return false;
      Square pos=p0.square();
      if (!pos.isOnBoard()) 
	return false;
      Piece p1=pieceAt(pos);
      int num1=p1.id();
      if (num0 !=num1) 
	return false;
    }
    else {
      Piece p0=pieceOf(num0);
      Ptype ptype=p0.ptype();

      if (p0.isEmpty() && piece_id_ptype[num0] == KING)
	continue;
      if (p0.id()!=num0 || ptype!=piece_id_ptype[num0] || ! p0.square().isPieceStand())
	return false;
    }
  }
  // mask
  for (Ptype ptype: piece_stand_order) {
    if (countPiecesOnStand(BLACK, ptype) != countPiecesOnStandBit(BLACK, ptype)
        || countPiecesOnStand(WHITE, ptype) != countPiecesOnStandBit(WHITE, ptype))
      return false;
  }
  // pawnMask;
  {
    CArray<BitXmask,2> pawnMask1 = {XNone, XNone};
    for (int num: to_range(PAWN)) {
      if (! isOnBoard(num)) continue;
      Piece p=pieceOf(num);
      if (!p.isPromoted())
        set_x(pawnMask1[p.owner()], p.square());
    }
    if ((pawnMask[0]!=pawnMask1[0]) || (pawnMask[1]!=pawnMask1[1]))
      return false;
  }
  // illegal position for piece
  for (int id: to_range(PAWN)) {
    const Piece pawn = pieceOf(id);
    if (! pawn.isPromoted() && pawn.isOnBoard()
	&& pawn.square().blackView(pawn.owner()).y() == 1) 
      return false;
  }
  for (int id: to_range(LANCE)) {
    const Piece lance = pieceOf(id);
    if (! lance.isPromoted() && lance.isOnBoard()
	&& lance.square().blackView(lance.owner()).y() == 1) 
      return false;
  }
  for (int id: to_range(KNIGHT)) {
    const Piece knight = pieceOf(id);
    if (! knight.isPromoted() && knight.isOnBoard()
	&& knight.square().blackView(knight.owner()).y() == 1)
      return false;
  }
  return true;
}

osl::BaseState osl::BaseState::rotate180() const {
  BaseState ret;
  for (int i: all_piece_id()) {
    if (! active_set.test(i))
      continue;
    const Piece p = pieceOf(i);
    ret.setPiece(alt(p.owner()), p.square().rotate180(), p.ptype());
  }
  ret.setTurn(alt(turn()));
  ret.initFinalize();
  return ret;
}

bool osl::BaseState::move_is_consistent(Move move) const {
  assert(move.is_ordinary_valid());

  if (this->turn() != move.player())
    return false;  

  const Square to=move.to(), from=move.from();
  if (from.isPieceStand()) { // 打つ手 isAlmostValidDrop(move);
    const Ptype ptype=move.ptype();
    // ターゲットが空白か そもそもその駒を持っているか? 二歩?
    return pieceAt(to).isEmpty() && hasPieceOnStand(turn(),ptype)
      && !(ptype==PAWN && pawnInFile(turn(), to.x()));
  }

  // move on board
  const Piece from_piece = this->pieceAt(from);
  if (from_piece.isEmpty() || (from_piece.owner() != turn())) // fromにあるのがその駒か
    return false;
  
  if (move.isPromotion()) { // promoteしている時にpromote可能か    
    if (from_piece.ptype() != unpromote(move.ptype())) // fromにあるのがその駒か
      return false;
    if (from_piece.isPromoted())
      return false;
  }
  else {    
    if (from_piece.ptype() != move.ptype()) // fromにあるのがその駒か
      return false;
  }
  const Piece to_piece=pieceAt(to);  
  if (!to_piece.isEmpty() && to_piece.owner()==turn()) // toにあるのが，相手の駒か空白か?
    return false;  
  if (to_piece.ptype()!=move.capturePtype()) // capturePtypeが一致しているか?
    return false;

  return true;
}

void osl::BaseState::make_move_unsafe(Move move) {
  if (turn() != move.player() || ! move.is_ordinary_valid())
    throw std::logic_error("unacceptable move in unsafe method");

  const Square from=move.from(), to=move.to();
  const auto side_to_move = this->turn();
  int num;                      /* to be initilized in both branches of drop and non-drop */
  if (from.isPieceStand()) {
    auto ptype = move.ptype();
    const mask_t mochigoma= standMask(side_to_move).to_ullong() & piece_id_set(ptype);
    assert(mochigoma);
    int num = std::countr_zero(mochigoma);
    mask_t num_one_hot=lowest_bit(mochigoma);
    Piece new_piece=pieceOf(num).drop(to);
    assert(0 <= num && num < 40 && num_one_hot == one_hot(num) && new_piece.id() == num);
    const auto ptypeO=new_piece.ptypeO();
    pieces[num] = new_piece;
    setBoard(to, new_piece);
    stand_mask[side_to_move] ^= PieceMask(num_one_hot);
    stand_count[side_to_move][basic_idx(ptype)]--;
    if (ptype==PAWN)
      setPawn(turn(),to);
  }
  else { // onboard
    Piece old_piece = pieceAt(from);
    num=old_piece.id();
    const Piece captured = pieceOnBoard(to);
    int move_promote_mask = move.promoteMask();
    Piece new_piece=old_piece.move((to-from), move_promote_mask);
    pieces[num] = new_piece;
    const auto old_ptypeo=old_piece.ptypeO(), new_ptypeo=new_piece.ptypeO();
    setBoard(from, Piece::EMPTY());
    setBoard(to, new_piece);
    if (captured != Piece::EMPTY()) {
      const int cid = captured.id();
      pieces[cid] = captured.captured();
      const mask_t cid_mask=one_hot(cid);
      stand_mask[side_to_move] ^= PieceMask(cid_mask);
      
      const auto capturePtypeO=captured.ptypeO();
      stand_count[side_to_move][basic_idx(unpromote(ptype(capturePtypeO)))]++;

      if (captured.ptype() == PAWN)
        clearPawn(alt(turn()),to);
    }
    // onboard moves reach here
    if (move_promote_mask && num < ptype_piece_id[Int(PAWN)].second)
      clearPawn(turn(),from);
  }
  changeTurn();  
}


bool osl::operator==(const BaseState& st1,const BaseState& st2) {
  assert(st1.check_internal_consistency());
  assert(st2.check_internal_consistency());
  if (st1.turn()!=st2.turn()) 
    return false;
  if (st1.active_set.countBit() != st2.active_set.countBit())
    return false;               // todo check ptype count if not 0
  for (auto pl: players) {
    if (st1.pawnMask[pl]!=st2.pawnMask[pl]) return false;
    if (st1.stand_count[pl] != st2.stand_count[pl]) return false;
  }
  for (int y: board_y_range())
    for (int x: board_x_range()) {
      Piece p1=st1.pieceAt(Square(x,y));
      Piece p2=st2.pieceAt(Square(x,y));
      if (p1.ptypeO()!=p2.ptypeO()) return false;
    }
  return true;
      
}

std::ostream& osl::operator<<(std::ostream& os,const BaseState& state)
{
  for (int y: board_y_range()) {
    os << 'P' << y;  
    for (int x: board_x_range()) {
      os << to_csa(state.pieceOnBoard(Square(x,y)));
    }
    os << std::endl;
  }
  PieceStand bs(BLACK, state), ws(WHITE, state);
  os << bs.to_csa(BLACK) << ws.to_csa(WHITE);
  os << state.turn() << std::endl;
  return os;
}

