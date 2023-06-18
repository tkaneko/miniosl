#include "base-state.h"
#include "record.h"
#include <iostream>

osl::BaseState::BaseState() {
  initEmpty();
}

osl::BaseState::BaseState(Handicap h) {
  init(h);
}

void osl::BaseState::initFinalize(){
  for (Ptype ptype: piece_stand_order) {
    stand_count[BLACK][basic_idx(ptype)] = countPiecesOnStandBit(BLACK, ptype);
    stand_count[WHITE][basic_idx(ptype)] = countPiecesOnStandBit(WHITE, ptype);
  }

  pawnMask[0] = XNone;
  pawnMask[1] = XNone;
  for (int num: to_range(PAWN)) {
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
  used_mask.resetAll();
  pawnMask[0] = XNone;
  pawnMask[1] = XNone;
  for (int num: all_piece_id()){
    pieces[num]=Piece(WHITE,piece_id_ptype[num],num,Square::STAND());
  }
}
  

void osl::BaseState::init(Handicap h) {
  initEmpty();
  if (h != HIRATE) {
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
  

osl::BaseState::~BaseState() {}

void osl::BaseState::setPiece(Player player,Square pos,Ptype ptype) {
  for (int num: all_piece_id()) {
    if (!used_mask.test(num) && piece_id_ptype[num]==unpromote(ptype)
	&& (ptype!=KING || 
	    num==king_piece_id(player))) {
      used_mask.set(num);
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
    if (!used_mask.test(num)) {
      used_mask.set(num);
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
	if (! is_valid_piece_id(num) || !used_mask.test(num)) 
	  return false;
	Piece p1=pieceOf(num);
	if (p0!=p1) 
	  return false;
      }
    }
  }
  // piecesのconsistency
  for (int num0: all_piece_id()) {
    if(!usedMask().test(num0)) continue;
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
    if(!usedMask().test(i)) continue;
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

bool osl::operator==(const BaseState& st1,const BaseState& st2) {
  assert(st1.check_internal_consistency());
  assert(st2.check_internal_consistency());
  if (st1.turn()!=st2.turn()) 
    return false;
  if (st1.used_mask.countBit() != st2.used_mask.countBit())
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

