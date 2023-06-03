#include "state.h"
#include "record.h"
#include "more.h"
#include "checkmate.h"
#include <iostream>
#include <iomanip>
#include <bitset>
#include <algorithm>

std::ostream& osl::operator<<(std::ostream& os,Player player) {
  return os << ("+-"[idx(player)]);
}
std::ostream& osl::operator<<(std::ostream& os,const osl::Ptype ptype) {
  return os << ptype_en_names[Int(ptype)];
}
std::istream& osl::operator>>(std::istream& is, osl::Ptype& ptype) {
  std::string s;
  is >> s;
  auto ptr = std::ranges::find(ptype_en_names, s);
  if (ptr == ptype_en_names.end()) {
    std::cerr << "Incorrect input : " << s << std::endl;
    ptype = Ptype_EMPTY;
    is.setstate(std::ios::failbit);
  }
  else {
    ptype = Ptype(ptr-ptype_en_names.begin());
  }
  return is;
}
std::ostream& osl::operator<<(std::ostream& os,const osl::PtypeO ptypeO) {
  if (is_piece(ptypeO))
    return os << "PtypeO(" << owner(ptypeO) << "," 
	      << ptype(ptypeO) << ")";
  return os << "PtypeO(" << (int)ptypeO << "," << ptype(ptypeO) << ")";
}
std::ostream& osl::operator<<(std::ostream& os,const Direction d){
  static const char* names[]={
    "UL","U","UR","L",
    "R","DL","D","DR",
    "UUL","UUR","Long_UL",
    "Long_U","Long_UR","Long_L",
    "Long_R","Long_DL","Long_D","Long_DR"
  };
  return os << names[static_cast<int>(d)];
}
std::ostream& osl::operator<<(std::ostream& os, Offset offset) {
  return os << "offset(" << Int(offset) << ')';
}



static_assert(sizeof(osl::Square) == 4, "square size");
bool osl::Square::isValid() const {
  return isPieceStand() || isOnBoard();
}

std::ostream& osl::operator<<(std::ostream& os, Square square) {
  if (square.isPieceStand())
    return os << "OFF";
  return os << "Square(" << square.x() << ',' << square.y() << ")";
}

static_assert(sizeof(osl::Piece) == 4, "piece size");

std::ostream& osl::operator<<(std::ostream& os,const Piece piece) {
  if (piece.isPiece())
    os << "Piece(" << piece.owner() << "," << piece.ptype() << "," << piece.square()
       << ",num=" << piece.id() << ')';
  else if (piece == Piece::EMPTY())
    os << "Piece_EMPTY";
  else if (piece == Piece::EDGE())
    os << "Piece_EDGE";
  else
    os << "unkown piece?!";
  return os;
}



namespace osl
{
  static_assert(sizeof(Move) == 4, "move size");
} //namespace osl

bool osl::Move::isValid() const {
  if (! isNormal())
    return false;
  const Square from = this->from(), to = this->to();
  if (! from.isValid() || ! to.isOnBoard())
    return false;
  return is_valid(ptype()) && is_valid(capturePtype()) && capturePtype()!=KING && is_valid(player());
}

bool osl::Move::isConsistent() const {
  if (! isValid())
    return false;
  const Square from=this->from(), to=this->to();
  const Ptype ptype=this->ptype();
  const Player turn = player();
    
  if (is_basic(ptype) && isPromotion())
    return false;

  if (from.isPieceStand()) {  // 打つ手
    // 動けない場所ではないか?
    return is_basic(ptype) && legal_drop_at(turn,ptype,to) && !isCapture();
  }
  
  const PtypeO old_ptypeo = oldPtypeO();
  const auto effect = ptype_effect(old_ptypeo, to_offset32(to,from));
  if (!is_definite(effect)) { // その offsetの動きがptypeに関してvalidか?
    if (to_offset(effect) == Offset_ZERO) 
      return false;
  }
  if (isPromotion()) { // promoteしている時にpromote可能か
    if (! (can_promote(osl::unpromote(ptype))
           && (to.isPromoteArea(player()) || from.isPromoteArea(player()))))
      return false;
  }
  if ((! is_promoted(ptype)
       && ! legal_drop_at(turn,osl::ptype(old_ptypeo),to)) 
      && !isPromotion()) { // promoteしていない時に強制promoteでないか?
    return false;
  }
  return true;
}

const osl::Move osl::Move::rotate180() const
{
  if (isPass())
    return Move::PASS(alt(player()));
  if (! isNormal())
    return *this;
  return Move(from().rotate180(), to().rotate180(), ptype(),
	      capturePtype(), isPromotion(), alt(player()));
}

std::ostream& osl::operator<<(std::ostream& os,const Move move)
{
  if (move == Move::DeclareWin())
    return os << "Move_Declare_WIN";
  if (move.isInvalid())
    return os << "Move_Resign";
  if (move.isPass())
    return os << "Move_Pass";
  const Player turn = move.player();
  if (move.isValid()) {
    if (move.from().isPieceStand())  {
      os << "Drop(" << move.to() << "," << move.ptype() << "," << turn << ")";
    }
    else {
      const Ptype capture_ptype=move.capturePtype();
      os << "Move(" << turn << "," << move.ptype() << "," 
	 << move.from() << "->" << move.to() ;
      if (move.promoteMask())
	os << ",promote";
      if (capture_ptype != Ptype_EMPTY)
	os << ",capture=" << capture_ptype;
      os << ")";
    }
  }
  else {
    os << "InvalidMove " << move.from() << " " << move.to() 
       << " " << move.ptypeO() << " " << move.oldPtypeO()
       << " " << move.promoteMask()
       << " " << move.capturePtype() << "\n";
  }
  return os;
}

std::ostream& osl::operator<<(std::ostream& os, const MoveVector& moves) {
  os << "MoveVector(" << moves.size() << ") ";
  for (auto m: moves)
    os << to_csa(m);
  return os << "\n";
}


namespace osl
{
  static_assert(sizeof(unsigned int)*/*CHARBITS*/8>=32, "PieceStand");

  const CArray<unsigned char,Ptype_MAX+1> PieceStand::shift
  = {{ 0,0,0,0,0,0,0,0, 28, 24, 18, 14, 10, 6, 3, 0, }};
  const CArray<unsigned char,Ptype_MAX+1> PieceStand::mask
  = {{ 0,0,0,0,0,0,0,0, (1<<2)-1, (1<<3)-1, (1<<5)-1, (1<<3)-1, (1<<3)-1, (1<<3)-1, (1<<2)-1, (1<<2)-1 }};
}

osl::PieceStand::
PieceStand(Player pl, const BaseState& state)
  : flags(0)
{
  for (Ptype ptype: piece_stand_order)
    add(ptype, state.countPiecesOnStand(pl, ptype));
}

bool osl::PieceStand::canAdd(Ptype type) const
{
  const auto [l, r] = ptype_piece_id[Int(type)];
  const int max = l-r;
  assert(max >= 0);
  return (static_cast<int>(get(type)) != max);
}

void osl::PieceStand::tryAdd(Ptype type)
{
  if (canAdd(type))
    add(type);
}

bool osl::PieceStand::atMostOneKind() const
{
  return std::popcount(getFlags()) <= 1;
}

#ifndef MINIMAL
bool osl::PieceStand::
carryUnchangedAfterAdd(const PieceStand& original, const PieceStand& other) const
{
  if (original.testCarries() == testCarries())
    return true;
  std::cerr << original << " + " << other << " = " << *this << "\n";
  return false;
}

bool osl::PieceStand::
carryUnchangedAfterSub(const PieceStand& original, const PieceStand& other) const
{
  if (original.testCarries() == testCarries())
    return true;
  std::cerr << original << " - " << other << " = " << *this << "\n";
  return false;
}

std::ostream& osl::operator<<(std::ostream& os, osl::PieceStand stand)
{
  os << "(stand";
  for (Ptype ptype: piece_stand_order)
  {
    os << ' ' << stand.get(ptype);
  }
  return os << ")";
}
#endif

std::ostream& osl::
PieceStandIO::writeNumbers(std::ostream& os, const PieceStand& stand)
{
  for (Ptype ptype: piece_stand_order) {
    os << stand.get(ptype) << " ";
  }
  return os;
}
std::istream& osl::
PieceStandIO::readNumbers(std::istream& is, PieceStand& stand)
{
  stand  = PieceStand();
  for (Ptype ptype: piece_stand_order) {
    int val;
    if (is >> val) 
      stand.add(ptype, val);
  }
  return is;
}

// pieceTable

// 

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
  assert(isConsistent());
}

void osl::BaseState::initEmpty() {
  player_to_move=BLACK;
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
    throw std::runtime_error("unsupported handicap");
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
  
bool osl::BaseState::isConsistent() const {
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

const osl::BaseState osl::BaseState::rotate180() const
{
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

bool osl::operator==(const BaseState& st1,const BaseState& st2)
{
  assert(st1.isConsistent());
  assert(st2.isConsistent());
  if (st1.turn()!=st2.turn()) 
    return false;
  if (st1.pawnMask[0]!=st2.pawnMask[0]) return false;
  if (st1.pawnMask[1]!=st2.pawnMask[1]) return false;
  for (int y: board_y_range())
    for (int x: board_x_range()) {
      Piece p1=st1.pieceAt(Square(x,y));
      Piece p2=st2.pieceAt(Square(x,y));
      if (p1.ptypeO()!=p2.ptypeO()) return false;
    }
  return true;
      
}

namespace osl
{
  namespace 
  {
    void showStand(std::ostream& os, Player player, PieceStand stand) {
      if (! stand.any())
	return;
      
      os << "P" << to_csa(player);
      for (Ptype ptype: piece_stand_order) {
	for (unsigned int j=0; j<stand.get(ptype); ++j) {
	  os << "00" << to_csa(ptype);
	}
      }
      os << "\n";
    }
  } // anonymous namespace
} // namespace osl

std::ostream& osl::operator<<(std::ostream& os,const BaseState& state)
{
  for (int y: board_y_range()) {
    os << 'P' << y;  
    for (int x: board_x_range()) {
      os << to_csa(state.pieceOnBoard(Square(x,y)));
    }
    os << std::endl;
  }
  // 持ち駒の表示
  const PieceStand black_stand(BLACK, state);
  const PieceStand white_stand(WHITE, state);
  showStand(os, BLACK, black_stand);
  showStand(os, WHITE, white_stand);
  
  os << state.turn() << std::endl;
  return os;
}

// pieceMask.cc
static_assert(sizeof(osl::PieceMask) == 8, "piecemask size");

#ifndef MINIMAL
std::ostream& osl::operator<<(std::ostream& os,const PieceMask& pieceMask){
  os << '(' << std::setbase(16) << std::setfill('0') 
	    << std::setw(12) << pieceMask.to_ullong()
	    << std::setbase(10) << ')';
  os << std::bitset<64>(pieceMask.to_ullong());
  return os;
}
#endif


// king8Info.cc
#ifndef MINIMAL
std::ostream& osl::checkmate::operator<<(std::ostream& os, King8Info info)
{
  typedef std::bitset<8> bs_t;
  os << bs_t(moveCandidate2(info)) << " " 
     << bs_t(libertyCandidate(info)) << " " 
     << bs_t(liberty(info)) << " " 
     << bs_t(dropCandidate(info));
  return os;
}
#endif
namespace osl
{
  namespace
  {
/**
 * Pの玉やpinされている駒以外からの利きがある.
 * @param state - 盤面(alt(P)の手番とは限らない)
 * @param target - Pの玉の位置
 * @param pos - 盤面上の(Pの玉から長い利きの位置にあるとは限らない)
 * @param pinned - pinされているPの駒のmask
 * @param on_baord_defense - alt(P)の盤面上の駒のうちkingを除いたもの
 * @param dir - dir方向へのattack
 */
template<Player Defense> inline
bool hasEnoughGuard(EffectState const& state,Square target,Square pos, PieceMask pinned,
		     PieceMask on_board_defense, Direction dir) {
  assert(state.kingSquare(Defense)==target);
  assert(pos.isOnBoard());
  PieceMask guards = state.effectAt(pos) & on_board_defense;
  if (guards.none()) return false;
  if ((guards&~pinned).any()) return true;
  guards&=pinned;
  for (int num: guards.toRange()) { // 最初のifは無駄ではある
    Piece p=state.pieceOf(num);
    assert(p.isOnBoardByOwner(Defense));
    Square pos1=p.square();
    assert(basic_step(to_offset32(pos,target)) == pos-target);
    if (base8_dir<Defense>(target,pos1) == dir) return true;
  } 
  return false;
}
  }
}

template<osl::Player Attack, osl::Direction Dir>
uint64_t osl::checkmate::
make_king8info(EffectState const& state,Square king, PieceMask pinned, PieceMask on_board_defense) {
  const Player Defense = alt(Attack);
  Square pos=king-to_offset(Attack, Dir);
  assert(pos.index() < Square::SIZE);
  Piece p=state.pieceAt(pos);
  if(p.isEdge())
    return 0ull;
  if (!state.hasEffectAt(Attack, pos)){
    if (p.canMoveOn<Defense>()){ // 攻撃側の駒か空白
      if(p.isEmpty())
	return 0x1000000000000ull+(0x100010100ull<<Int(Dir));
      else
	return 0x1000000000000ull+(0x10100ull<<Int(Dir));
    }
    else // 玉側の駒
      return 0ull;
  }
  const bool has_enough_guard = hasEnoughGuard<Defense>(state,king,pos,pinned,on_board_defense,Dir);
  if(has_enough_guard){
    if(p.canMoveOn<Defense>()){
      if(p.isEmpty())
	return 0x10100010000ull<<Int(Dir);
      else
	return 0x10000ull<<Int(Dir);
    }
    else
      return 0x10000000000ull<<Int(Dir);
  }
  else{
    if(p.isEmpty())
      return 0x10101010001ull<<Int(Dir);
    else if(p.isOnBoardByOwner<Attack>())
      return 0x10000ull<<Int(Dir);
    else
      return 0x10001000000ull<<Int(Dir);
  }
}

template<osl::Player Attack>
osl::checkmate::King8Info 
osl::checkmate::to_king8info(EffectState const& state, Square king, PieceMask pinned)
{
  PieceMask on_board_defense=state.piecesOnBoard(alt(Attack));
  on_board_defense.reset(king_piece_id(alt(Attack)));
  uint64_t canMoveMask = make_king8info<Attack,UR>(state, king, pinned,on_board_defense)
    + make_king8info<Attack,R> (state, king, pinned, on_board_defense)
    + make_king8info<Attack,DR>(state, king, pinned, on_board_defense)
    + make_king8info<Attack,U> (state, king, pinned, on_board_defense)
    + make_king8info<Attack,D> (state, king, pinned, on_board_defense)
    + make_king8info<Attack,UL>(state, king, pinned, on_board_defense)
    + make_king8info<Attack,L> (state, king, pinned, on_board_defense)
    + make_king8info<Attack,DL>(state, king, pinned, on_board_defense);
  for (auto num: to_range(state.longEffectAt(king, Attack))){
    Piece attacker=state.pieceOf(num);
    Direction d= base8_dir<Attack>(king, attacker.square());
    if((canMoveMask&(0x100<<Int(d)))!=0)
      canMoveMask-=((0x100<<Int(d))+0x1000000000000ull);
  }
  return King8Info(canMoveMask);
}


// numSimpleEffect.tcc
template<osl::Player P, osl::EffectOp OP>
void  osl::effect::
EffectSummary::doEffect(const BaseState& state,PtypeO ptypeo,Square pos,int num)
{
  assert(P == owner(ptypeo));
  switch((int)ptypeo){
  case newPtypeO(P,PAWN):   doEffectBy<P,PAWN,OP>(state,pos,num); break;
  case newPtypeO(P,LANCE):  doEffectBy<P,LANCE,OP>(state,pos,num); break;
  case newPtypeO(P,KNIGHT): doEffectBy<P,KNIGHT,OP>(state,pos,num); break;
  case newPtypeO(P,SILVER): doEffectBy<P,SILVER,OP>(state,pos,num); break;
  case newPtypeO(P,PPAWN):
  case newPtypeO(P,PLANCE):
  case newPtypeO(P,PKNIGHT):
  case newPtypeO(P,PSILVER):
  case newPtypeO(P,GOLD):    doEffectBy<P,GOLD,OP>(state,pos,num); break;
  case newPtypeO(P,BISHOP):  doEffectBy<P,BISHOP,OP>(state,pos,num); break;
  case newPtypeO(P,PBISHOP): doEffectBy<P,PBISHOP,OP>(state,pos,num); break;
  case newPtypeO(P,ROOK):    doEffectBy<P,ROOK,OP>(state,pos,num); break;
  case newPtypeO(P,PROOK):   doEffectBy<P,PROOK,OP>(state,pos,num); break;
  case newPtypeO(P,KING):    doEffectBy<P,KING,OP>(state,pos,num); break;
  default: assert(0);
  }
  return;
}

template<osl::Player P, osl::Ptype T, osl::EffectOp OP>
void  osl::effect::
EffectSummary::doEffectBy(const BaseState& state,Square pos,int num)
{
  if constexpr (T==LANCE || T==BISHOP || T==PBISHOP || T==ROOK || T==PROOK)
    setSourceChange(EffectPieceMask::makeLong<P>(num));
  else
    setSourceChange(EffectPieceMask::make<P>(num));

  doEffectShort<P,T,UL,OP>(state,pos,num);
  doEffectShort<P,T,U,OP>(state,pos,num);
  doEffectShort<P,T,UR,OP>(state,pos,num);
  doEffectShort<P,T,L,OP>(state,pos,num);
  doEffectShort<P,T,R,OP>(state,pos,num);
  doEffectShort<P,T,DL,OP>(state,pos,num);
  doEffectShort<P,T,D,OP>(state,pos,num);
  doEffectShort<P,T,DR,OP>(state,pos,num);
  doEffectShort<P,T,UUL,OP>(state,pos,num);
  doEffectShort<P,T,UUR,OP>(state,pos,num);
  doEffectLong<P,T,Long_UL,OP>(state,pos,num);
  doEffectLong<P,T,Long_U,OP>(state,pos,num);
  doEffectLong<P,T,Long_UR,OP>(state,pos,num);
  doEffectLong<P,T,Long_L,OP>(state,pos,num);
  doEffectLong<P,T,Long_R,OP>(state,pos,num);
  doEffectLong<P,T,Long_DL,OP>(state,pos,num);
  doEffectLong<P,T,Long_D,OP>(state,pos,num);
  doEffectLong<P,T,Long_DR,OP>(state,pos,num);
}

template<osl::Player P, osl::Ptype T, osl::Direction Dir, osl::EffectOp OP>
void  osl::effect::EffectSummary::
doEffectShort(const BaseState& state,Square pos,int num) {
  if constexpr (! bittest(ptype_move_direction[idx(T)], Dir))
    return;
        
  const Square target = pos + to_offset(P,Dir);
  e_squares[target].increment<OP>(EffectPieceMask::make<P>(num));

  board_modified[P].setNeighbor<Dir,P>(pos);
  int num1=state.pieceAt(target).id();
  if (Piece::isPieceNum(num1)) {
    if constexpr (OP==EffectAdd)
      e_pieces[P].set(num1);
    else{ // OP==Sub
      if (! e_squares[target].hasEffect<P>())
        e_pieces[P].reset(num1);
    }
    e_pieces_modified[P].set(num1);
  }
}
template<osl::Player P, osl::Ptype T, osl::Direction Dir, osl::EffectOp OP>
void  osl::effect::EffectSummary::
doEffectLong(const BaseState& state,Square pos,int num) {
  if constexpr (! bittest(ptype_move_direction[idx(T)], change_view(P,Dir)))
    return;

  int index_b=BoardMask::index(pos);
  constexpr Offset offset=to_offset(BLACK,Dir);
  assert(offset != Offset_ZERO);
  auto effect=EffectPieceMask::makeLong<P>(num);

  constexpr Direction SD=long_to_base8(Dir);
  if constexpr (OP==EffectSub) {
    Square dst=long_piece_reach.get(SD, num);
    long_piece_reach.set(SD, num, Square::STAND());
    int count=((SD==D || SD==DL || SD==DR) ? dst.y()-pos.y() :
               ((SD==U || SD==UL || SD==UR) ? pos.y()-dst.y() :
                (SD==L ? dst.x()-pos.x() : pos.x()-dst.x())));
    assert(0<=count && count<=9);

    for(int i=1;i<count;i++) {
      pos+=offset;
      BoardMask::advance<Dir,BLACK>(index_b);
      e_squares[pos].increment<OP>(effect);
      board_modified[P].set(index_b);
    }
    int num1=state.pieceAt(dst).id();
    if (!Piece::isEdgeNum(num1)) {
      pp_long_state[num1][SD]=Piece_ID_EMPTY;
      e_squares[dst].increment<OP>(effect);
      e_pieces_modified[P].set(num1);
      BoardMask::advance<Dir,BLACK>(index_b);
      board_modified[P].set(index_b);
      if(! e_squares[dst].hasEffect<P>()) 
        e_pieces[P].reset(num1);
    }
  }
  else{ // OP==Add
    for (;;) {
      pos += offset;
      BoardMask::advance<Dir,BLACK>(index_b);
      board_modified[P].set(index_b);

      e_squares[pos].increment<OP>(effect);
      // effect内にemptyを含むようにしたら短くなる
      int num1=state.pieceAt(pos).id();
      if (!Piece::isEmptyNum(num1)) {
        long_piece_reach.set(SD, num, pos);
        if (!Piece::isEdgeNum(num1)) {
          pp_long_state[num1][SD]=num;
          board_modified[P].set(index_b);
          e_pieces[P].set(num1); // num1
          e_pieces_modified[P].set(num1);
        }
        break;
      }
    }
  }
}
template<osl::EffectOp OP>
void osl::effect::
EffectSummary::doBlockAt(const BaseState& state,Square pos,int piece_num) {
  setSourceChange(e_squares[pos]);

  for (int src_id: long_to_piece_id_range(e_squares[pos].selectLong())) {
    assert(32<=src_id && src_id<=39);
    auto p_src=state.pieceOf(src_id);
    auto pl_src=p_src.owner();
    auto effect=EffectPieceMask::makeLong(pl_src,src_id);
    auto [d, offset0]=base8_dir_step<BLACK>(p_src.square(), pos);
    Square pos2=pos+offset0;
    int index_2b=BoardMask::index(pos2);
    int offset81=index_2b - BoardMask::index(pos);
    // p_src(src_id) -- pos(piece_num) <-- pos2 --> dst(num_dst)
    if constexpr (OP==EffectSub) {
      Square dst=long_piece_reach.get(d,src_id); 
      for (;pos2!=dst; pos2+=offset0, index_2b+=offset81) {
        board_modified[pl_src].set(index_2b);
	e_squares[pos2].increment<OP>(effect);
      }
      e_squares[pos2].increment<OP>(effect);
      int num_dst=state.pieceAt(dst).id();
      if (!Piece::isEdgeNum(num_dst)) {
	pp_long_state[num_dst][d]=Piece_ID_EMPTY;

        board_modified[pl_src].set(index_2b);
        if (! e_squares[dst].hasEffect(pl_src))
          e_pieces[pl_src].reset(num_dst);
        e_pieces_modified[pl_src].set(num_dst);
      }
      long_piece_reach.set(d,src_id,pos);
      pp_long_state[piece_num][d]=src_id;
    }
    else{
      int num2=state.pieceAt(pos2).id();
      for (;Piece::isEmptyNum(num2); 
           pos2+=offset0, index_2b+=offset81, num2=state.pieceAt(pos2).id()) {
        board_modified[pl_src].set(index_2b);
	e_squares[pos2].increment<OP>(effect);
      }
      long_piece_reach.set(d,src_id,pos2);
      if (!Piece::isEdgeNum(num2)) {
        pp_long_state[num2][d]=src_id;
        e_squares[pos2].increment<OP>(effect);
        board_modified[pl_src].set(index_2b);
        e_pieces[pl_src].set(num2);
        e_pieces_modified[pl_src].set(num2);
      }
    }
  }
}

// numSimpleEffect.cc
void osl::effect::
EffectSummary::init(const BaseState& state)
{
  std::fill(e_squares.begin(), e_squares.end(),EffectPieceMask());
  pp_long_state.clear();
  long_piece_reach.clear();
  
  for (int num: all_piece_id()) {
    if (state.isOnBoard(num)) {
      Piece p=state.pieceOf(num);
      doEffect<EffectAdd>(state,p);
    }
  }
}

void osl::effect::
EffectSummary::copyFrom(const EffectSummary& src)
{
  this->e_pieces = src.e_pieces;
  this->long_piece_reach = src.long_piece_reach;
  this->board_modified = src.board_modified;
  this->source_pieces_modified = src.source_pieces_modified;
  this->e_pieces_modified = src.e_pieces_modified;

  this->pp_long_state = src.pp_long_state;

  for(int y: board_y_range())
    for(int x: board_x_range())
      this->e_squares[Square(x,y)]=src.e_squares[Square(x,y)];
}

bool osl::effect::operator==(const EffectSummary& et1,const EffectSummary& et2)
{
  for(int y: board_y_range())
    for(int x: board_x_range()) {
      Square pos(x,y);
      if (!(et1.effectAt(pos)==et2.effectAt(pos))) return false;
    }
  if (! (et1.e_pieces == et2.e_pieces))
    return false;
  if(!(et1.long_piece_reach==et2.long_piece_reach)) return false;
  if(!(et1.pp_long_state==et2.pp_long_state)) return false;
  // intentionally ignore history dependent members: board_modified, source_pieces_modified, e_pieces_modified
  return true;
}

#ifndef MINIMAL
std::ostream& osl::effect::operator<<(std::ostream& os,const EffectSummary& effectTable)
{
  os << "Effect" << std::endl;
  for(int y: board_y_range()) {
    for(int x: board_y_range()){
      Square pos(x,y);
      os << effectTable.effectAt(pos) << " ";
    }
    os << std::endl;
  }
  os << "Effect" << std::endl;
  for(int y: board_y_range()){
    for(int x: board_x_range()){
      Square pos(x,y);
      os << effectTable.effectAt(pos) << " ";
    }
    os << std::endl;
  }
  return os;
}
#endif

// effectedNumTable.cc
// mobilityTable.cc

// boardMask.cc
#ifndef MINIMAL
std::ostream& osl::operator<<(std::ostream& os, const BoardMask& mask)
{
  for(int y=1; y<=9; ++y) {
    for(int x=9; x>=1; --x) {
      const Square p(x,y);
      os << mask.test(p);
    }
    os << std::endl;
  }
  return os;
}
#endif

// numEffectState.cc
bool osl::operator==(const EffectState& st1, const EffectState& st2)
{
  assert(st1.isConsistent());
  assert(st2.isConsistent());
  if (!(st1.effects == st2.effects)) 
    return false;
  if (!(st1.pieces_onboard == st2.pieces_onboard)) 
    return false;
  if (!(st1.promoted == st2.promoted)) 
    return false;
  if (!(st1.pin_or_open == st2.pin_or_open)) 
    return false;
  if (!(st1.king_visibility == st2.king_visibility)) 
    return false;
  if (!(st1.king8infos == st2.king8infos)) 
    return false;
  return (static_cast<const BaseState&>(st1)
	  == static_cast<const BaseState&>(st2));
}

template<osl::Player P>
void osl::EffectState::makeKing8Info()
{
  king8infos[P] = King8Info{0};
  if (kingSquare<P>().isPieceStand())
    return;
  king8infos[P]=to_king8info<alt(P)>(*this,kingSquare<P>());
}

osl::
EffectState::EffectState(const BaseState& st) 
  : BaseState(st),effects(st)
{
  pieces_onboard[0].resetAll();
  pieces_onboard[1].resetAll();
  promoted.resetAll();
  effects.e_pieces[0].resetAll();
  effects.e_pieces[1].resetAll();
  effects.e_pieces_modified[0].resetAll();
  effects.e_pieces_modified[1].resetAll();
  for (int num: all_piece_id()) {
    Piece p=pieceOf(num);
    if (p.isOnBoard()){
      pieces_onboard[p.owner()].set(num);
      if (p.isPromoted())
	promoted.set(num);
      for (auto pl: players){
	if(hasEffectAt(pl,p.square()))
          {
            effects.e_pieces[pl].set(num);
            effects.e_pieces_modified[pl].set(num);
          }
      }
    }
  }
  setPinOpen(BLACK);
  setPinOpen(WHITE);
  makeKing8Info<BLACK>();
  makeKing8Info<WHITE>();
}
osl::
EffectState::~EffectState() 
{
}

osl::Piece osl::
EffectState::selectCheapPiece(PieceMask effect) const
{
  if (! effect.any())
    return Piece::EMPTY();
  mask_t pieces = effect.selectBit<PAWN>(), ppieces;
  if (pieces) {
    ppieces = pieces & promoted.to_ullong();
    pieces &= ~ppieces;
    if (pieces)
      return pieceOf(std::countr_zero(pieces));
    return pieceOf(std::countr_zero(ppieces));
  }
  pieces = effect.selectBit<LANCE>();
  if (pieces) {
    ppieces = pieces & promoted.to_ullong();
    pieces &= ~ppieces;
    if (pieces)
      return pieceOf(std::countr_zero(pieces));
    return pieceOf(std::countr_zero(ppieces));
  }
  mask_t king = effect.selectBit<KING>();
  effect.clearBit<KING>();
  if (effect.none())
    return pieceOf(std::countr_zero(king));
  // depends on current piece numbers: <FU 0>, KE 18, GI 22, KI 26, <OU 30>, <KY 32>, KA 36, HI 38, 
  ppieces = (effect & promoted).to_ullong();
  pieces = effect.to_ullong() & ~ppieces;
  if (pieces == 0 || ppieces == 0)
    return pieceOf(std::countr_zero(pieces ? pieces : ppieces));
  const int num = std::countr_zero(pieces), nump = std::countr_zero(ppieces);
  if (piece_id_ptype[num] == piece_id_ptype[nump])
    return pieceOf(num);
  return pieceOf(std::min(num, nump));
}

inline int bsr64(uint64_t val) 
{
  return 63-std::countl_zero(val);
}

const osl::Piece osl::
EffectState::findThreatenedPiece(Player P) const
{
  assert(! inCheck(P));
  PieceMask pieces = piecesOnBoard(P) & effectedPieces(alt(P));
  PieceMask nolance = pieces; nolance.clearBit<LANCE>();
  int pp=-1, npp=-1, ret=-1;
  {
    mask_t all = nolance.to_ullong();
    mask_t promoted = all & promotedPieces().to_ullong();
    mask_t notpromoted = all & ~promoted;
    if (promoted) {
      pp = bsr64(promoted);
      notpromoted &= ~ piece_id_set(piece_id_ptype[pp]);
    }
    if (notpromoted)
      npp = bsr64(notpromoted);
    ret = std::max(pp, npp);
    if (ret >= ptype_piece_id[Int(KNIGHT)].first)
      return pieceOf(ret);  
  }
  mask_t lance = pieces.selectBit<LANCE>();
  if (lance) {
    mask_t plance = lance & promotedPieces().to_ullong();
    if (plance)
      return pieceOf(bsr64(plance));
    return pieceOf(bsr64(lance));
  }
  if (ret >= 0) {
    assert(piece_id_ptype[ret] == PAWN);
    return pieceOf(ret);
  }
  return Piece::EMPTY();
}

void osl::EffectState::makeMove(Move move)
{
  assert(turn() == move.player());
  effects.clearPast();

  if (move.isPass()) {
    makeMovePass();
    return;
  }

  assert(isLegalLight(move));
  const Square from=move.from(), to=move.to();
  const auto pin_or_open_backup = pin_or_open;
  const auto player_to_move = this->turn();
  int num;                      /* to be initilized in both branches of drop and non-drop */
  if (from.isPieceStand()) {
    auto ptype = move.ptype();
    num = (player_to_move == BLACK) ? doDropMove<BLACK>(to,ptype) : doDropMove<WHITE>(to,ptype);
    recalcPinOpen(to, BLACK);
    recalcPinOpen(to, WHITE);
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
    if (captured != Piece::EMPTY()) {
      const int captured_id = captured.id();
      pieces[captured_id] = captured.captured();
      effects.setSourceChange(effectAt(to));
      if (player_to_move == BLACK)
        doCaptureMove<BLACK>(from,to,captured,move_promote_mask, old_piece, new_piece, num, captured_id);
      else
        doCaptureMove<WHITE>(from,to,captured,move_promote_mask, old_piece, new_piece, num, captured_id);
      promoted.reset(captured_id);
      effects.e_pieces[BLACK].reset(captured_id);
      effects.e_pieces[WHITE].reset(captured_id);
      if (captured.ptype() == PAWN)
        clearPawn(alt(turn()),to);
    }
    else {
      if (player_to_move == BLACK)
        doSimpleMove<BLACK>(from,to,move_promote_mask, old_piece, new_piece, num);
      else
        doSimpleMove<WHITE>(from,to,move_promote_mask, old_piece, new_piece, num);
    }
    // onboard moves reach here
    if (move_promote_mask) {
      promoted.set(num);
      if (num < ptype_piece_id[Int(PAWN)].second)
        clearPawn(turn(),from);
    }
  }
  // all moves reach
  if(hasEffectAt(BLACK,to))
    effects.e_pieces[BLACK].set(num);
  else
    effects.e_pieces[BLACK].reset(num);
  if (hasEffectAt(WHITE,to))
    effects.e_pieces[WHITE].set(num);
  else
    effects.e_pieces[WHITE].reset(num);
  effects.e_pieces_modified[BLACK].set(num);
  effects.e_pieces_modified[WHITE].set(num);

  BoardMask changed = changedEffects();
  changed.set(from);
  changed.set(to);
  if (changed.anyInRange(kingArea3x3<BLACK>()) || pin_or_open[BLACK]!=pin_or_open_backup[BLACK])
    makeKing8Info<BLACK>();
  if (changed.anyInRange(kingArea3x3<WHITE>()) || pin_or_open[WHITE]!=pin_or_open_backup[WHITE])
    makeKing8Info<WHITE>();

  changeTurn();
}

template<osl::Player P>
void osl::EffectState::
doSimpleMove(Square from, Square to, int promoteMask, Piece old_piece, Piece new_piece, int num)
{
  const PtypeO old_ptypeo=old_piece.ptypeO(), new_ptypeo=new_piece.ptypeO();
  
  // 自分自身の効きを外す
  effects.doEffect<P,EffectSub>(*this, old_ptypeo, from, num);
  // 自分自身がブロックしていたpromote?の延長
  // あるいは自分自身のブロック
  effects.pp_long_state.clear(num);
  setBoard(to, new_piece);
  effects.doBlockAt<EffectSub>(*this, to, num);
  setBoard(from, Piece::EMPTY());
  effects.doBlockAt<EffectAdd>(*this, from, num);
  effects.doEffect<P,EffectAdd>(*this, new_ptypeo, to, num);

  if (old_ptypeo == newPtypeO(P, KING))
    setPinOpen(P);
  else {
    pin_or_open[P].reset(num);
    updatePinOpen(from, to, P);
  }
  pin_or_open[alt(P)].reset(num);
  updatePinOpen(from, to, alt(P));
}

template<osl::Player P>
void osl::EffectState::
doCaptureMove(Square from, Square to, Piece target, int promoteMask, Piece old_piece, 
              Piece new_piece, int num0, int num1) {
  const mask_t num1Mask=one_hot(num1);
  pieces_onboard[alt(P)] ^= PieceMask(num1Mask);
  stand_mask[P] ^= PieceMask(num1Mask);

  const auto old_ptypeo=old_piece.ptypeO(), new_ptypeo=new_piece.ptypeO();
  const auto capturePtypeO=target.ptypeO();
  stand_count[P][basic_idx(unpromote(ptype(capturePtypeO)))]++;
  effects.doEffect<alt(P),EffectSub>(*this, capturePtypeO, to, num1);
  effects.doEffect<P,EffectSub>(*this, old_ptypeo, from, num0);
  setBoard(from,Piece::EMPTY());
  effects.doBlockAt<EffectAdd>(*this, from, num0);
  effects.pp_long_state[num0]=effects.pp_long_state[num1];
  effects.pp_long_state.clear(num1);
  setBoard(to,new_piece);
  effects.doEffect<P,EffectAdd>(*this, new_ptypeo, to, num0);

  if (old_ptypeo == newPtypeO(P, KING))
    setPinOpen(P);
  else {
    pin_or_open[P].reset(num0);
    pin_or_open[P].reset(num1); // captured is not pin
    updatePinOpen(from, to, P);
  }
  pin_or_open[alt(P)].reset(num0);
  pin_or_open[alt(P)].reset(num1); // captured is not pin
  updatePinOpen(from, to, alt(P));
}

template<osl::Player P>
int osl::EffectState::
doDropMove(Square to, Ptype ptype)
{
  const mask_t mochigoma= standMask(P).to_ullong() & piece_id_set(ptype);
  assert(mochigoma);
  int num = std::countr_zero(mochigoma);
  mask_t num_one_hot=lowest_bit(mochigoma);
  Piece new_piece=pieceOf(num).drop(to);
  assert(0 <= num && num < 40 && num_one_hot == one_hot(num) && new_piece.id() == num);
  const auto ptypeO=new_piece.ptypeO();
  pieces[num] = new_piece;
  effects.doBlockAt<EffectSub>(*this, to, num);
  effects.doEffect<P,EffectAdd>(*this, ptypeO, to, num);
  setBoard(to, new_piece);
  stand_mask[P] ^= PieceMask(num_one_hot);
  stand_count[P][basic_idx(ptype)]--;
  pieces_onboard[P] ^= PieceMask(num_one_hot);
  return num;
}

bool osl::EffectState::isConsistent() const {
  if (!BaseState::isConsistent()) 
    return false;
  EffectSummary effects1(*this);
  if (!(effects1==effects)) 
    return false;
  for (Player p: players) {
    if (kingSquare(p).isPieceStand())
      continue;
    King8Info king8info2 = to_king8info(alt(p), *this);
    if (king8Info(p) != king8info2) 
      return false;
  }
  for (Piece p: all_pieces()) {
    if (p.isOnBoard()) 
      if (promoted.test(p.id()) != p.isPromoted()) 
	return false;
  }
  for (auto p: long_pieces()) {
    if (! p.isOnBoard() || p.ptype() == PLANCE)
      continue;
    for (auto d: base8_directions()) { // black view
      auto dst = effects.long_piece_reach.get(d, p.id());
      if (dst.isPieceStand())
        continue;
      auto target = pieceAt(dst);
      auto src = p.square();
      auto step = to_offset(BLACK, d);
      if (! bittest(ptype_move_direction[idx(unpromote(p.ptype()))], change_view(p.owner(), to_long(d)))) // player_view
        continue;
      auto step2 = basic_step(to_offset32(dst-step, src));

      if (src+step != dst)
        if (step != step2 || ! isEmptyBetween(src, dst-step, step))
          return false;
      if (target.isPiece()) {
        if (!hasEffectByPiece(p, dst) || effects.pp_long_state[target.id()][d] != p.id())
          return false;
      }
    }
  }
  for (auto p: all_pieces()) {
    if (! p.isOnBoard())
      continue;
    for (auto d: base8_directions()) { // black view
      auto attack = effects.pp_long_state[p.id()][d];
      if (attack == Piece_ID_EMPTY)
        continue;
      auto attack_piece = pieceOf(attack);
      if (! attack_piece.isOnBoard() || ! hasEffectByPiece(attack_piece, p.square()))
        return false;
    }
  }
  for (auto pl: players) {
    auto king = kingSquare(pl);
    if (! king.isOnBoard())
      continue;
    for (auto d: base8_directions()) { // black view
      Square reach = kingVisibilityBlackView(pl, d);
      auto step = to_offset(BLACK, d); // view: reach -> king
      if (king == reach+step) continue;
      auto step2 = basic_step(to_offset32(king, reach+step));
      if (step != step2 || ! isEmptyBetween(reach+step, king, step))
        return false;      
    }
  }
  return true;
}

bool osl::EffectState::isLegalLight(Move move) const {
  if (move == Move::DeclareWin())
    return win_if_declare(*this);

  assert(this->turn() == move.player());
  assert(move.isConsistent());

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
  
  if (!hasEffectByPiece(from_piece,to)) {
    return false;
  }
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
bool osl::EffectState::isLegal(Move move) const {
  if (turn() != move.player() || ! move.isConsistent())
    return false;
  return isLegalLight(move) && isSafeMove(move) && ! isPawnDropCheckmate(move);
}

void osl::EffectState::
setPinOpen(Player defense){
  PieceMask pins;
  auto king = kingSquare(defense);
  if (king.isPieceStand()) {
    pin_or_open[defense] = pins;
    return;
  }
  PieceMask attack = piecesOnBoard(alt(defense));
  auto& km_side = king_visibility[defense];
  makePinOpenDir<UL>(king, pins, attack, km_side);
  makePinOpenDir<U> (king, pins, attack, km_side);
  makePinOpenDir<UR>(king, pins, attack, km_side);
  makePinOpenDir<L> (king, pins, attack, km_side);
  makePinOpenDir<R> (king, pins, attack, km_side);
  makePinOpenDir<DL>(king, pins, attack, km_side);
  makePinOpenDir<D> (king, pins, attack, km_side);
  makePinOpenDir<DR>(king, pins, attack, km_side);
  pin_or_open[defense] = pins;
}

void osl::EffectState::copyFrom(const EffectState& src)
{
#ifndef MINIMAL
  this->used_mask=src.used_mask;
#endif
  this->stand_mask=src.stand_mask;
  for(int y: board_y_range())
    for(int x: board_x_range())
      this->board[Square(x,y)]=src.board[Square(x,y)];
  this->pieces=src.pieces;

  this->pawnMask=src.pawnMask;
  this->stand_count = src.stand_count;
  this->player_to_move=src.player_to_move;
  effects.copyFrom(src.effects);
  this->pieces_onboard=src.pieces_onboard;
  this->promoted=src.promoted;
  this->pin_or_open=src.pin_or_open;
  this->king_visibility=src.king_visibility;
  this->king8infos=src.king8infos;
}

bool osl::EffectState::isSafeMove(Move move) const { return is_safe(*this, move); }
bool osl::EffectState::isCheck(Move move) const { return is_check(*this, move); }
bool osl::EffectState::isPawnDropCheckmate(Move move) const {
  return is_pawn_drop_checkmate(*this, move);
}
bool osl::EffectState::isDirectCheck(Move move) const { return is_direct_check(*this, move); }
bool osl::EffectState::isOpenCheck(Move move) const { return is_open_check(*this, move); }

void osl::EffectState::generateLegal(MoveVector& moves) const {
  moves.clear();
  moves.reserve(Move::MaxUniqMoves);
  if (inCheck()) {
    // 王手がかかっている時は防ぐ手のみを生成, 王手回避は不成も生成
    GenerateEscapeKing::generate(*this, moves);
  }
  else {
    // そうでなければ全ての手を生成
    MoveVector all_moves;
    all_moves.reserve(Move::MaxUniqMoves);
    MoveStore store(all_moves);
    move_generator::AllMoves::generate(turn(), *this, store);
    // この指手は，玉の素抜きがあったり，打歩詰の可能性があるので確認が必要
    for (auto move: all_moves) {
      if (! isSafeMove(move) || isPawnDropCheckmate(move)) {
        // std::cerr << "filter " << move << " by " << !isSafeMove(move) << isPawnDropCheckmate(move) <<'\n';
        continue;
      }
      moves.push_back(move);
    }
  }
}

bool osl::EffectState::inCheckmate() const {
  if (! inCheck())
    return false;
  MoveVector moves;
  generateLegal(moves);
  return moves.size() == 0;
}

void osl::EffectState::generateCheck(MoveVector& moves) const
{  
  moves.clear();
  using namespace osl::move_generator;
  if (! inCheck()) {
    MoveStore store(moves);
    Player player = turn();
    Square target = kingSquare(alt(turn()));
    bool has_pawn_checkmate=false;
  
    if(player==BLACK)
      AddEffect::generate<BLACK>(*this,target,store,has_pawn_checkmate);
    else
      AddEffect::generate<WHITE>(*this,target,store,has_pawn_checkmate);
  }
  else {
    MoveVector all_moves;
    GenerateEscapeKing::generate(*this, all_moves);
    for (auto move: all_moves)
      if (isCheck(move))
        moves.push_back(move);
  }
}

void osl::EffectState::generateWithFullUnpromotions(MoveVector& moves) const {
  generateLegal(moves);
  if (inCheck())
    return;
  for (int i=0, iend=moves.size(); i<iend; ++i) {
    const Move move = moves[i];
    if (move.hasIgnoredUnpromote())
      moves.push_back(move.unpromote());
  }
}

osl::Move osl::EffectState::tryCheckmate1ply() const {
  auto best_move=Move::PASS(turn());
  if (! inCheck())
    ImmediateCheckmate::hasCheckmateMove(turn(),*this,best_move);
  return best_move;
}

void osl::EffectState::
findEffect(Player P, Square target, PieceVector& out) const
{
  effect_action::StorePiece store(&out);
  forEachEffect(P, target, store);
}

template <osl::Player P>
bool osl::EffectState::
hasEffectByWithRemove(Square target,Square removed) const
{
  const Piece piece = pieceAt(removed);
  if (! piece.isPiece()) 
    return hasEffectAt<P>(target);
  if (piece.owner() == P) {
    if (hasEffectNotBy(P, piece, target))
      return true;
  }
  else {
    if (hasEffectAt(P, target))
      return true;
  }
  if (! longEffectAt(removed, P))
    return false;
  const Direction d = to_long_direction<BLACK>(to_offset32(target,removed));
  if (!is_long(d))
    return false;
  const int num=ppLongState()[piece.id()][long_to_base8(d)];
  return (! Piece::isEmptyNum(num)
	  && pieceOf(num).owner()==P);
}

namespace osl
{
  template <Player P>
  struct SafeCapture
  {
  public:
    const EffectState& state;
    Piece safe_one;
    SafeCapture(const EffectState& s) : state(s), safe_one(Piece::EMPTY()) {
    }
    void operator()(Piece effect_piece, Square target) {
      if (move_classifier::KingOpenMove::isMember<P>
	  (state, effect_piece.ptype(), effect_piece.square(), target))
	return;
      safe_one = effect_piece;
    }
  };
} // namespace osl

template <osl::Player P>
osl::Piece
osl::EffectState::safeCaptureNotByKing(Square target, Piece king) const
{
  assert(king.owner() == P);
  assert(king.ptype() == KING);
  PieceMask ignore = pin(P);
  ignore.set(king.id());
  const Piece piece = findAttackNotBy(P, target, ignore);
  if (piece.isPiece())
    return piece;
  SafeCapture<P> safe_captures(*this);
  this->forEachEffectNotBy<P>(target, king, safe_captures);
  
  return safe_captures.safe_one;
}

namespace osl
{
  // explicit template instantiation
  template checkmate::King8Info checkmate::to_king8info<BLACK>(EffectState const&, Square, PieceMask);
  template checkmate::King8Info checkmate::to_king8info<WHITE>(EffectState const&, Square, PieceMask);

  template bool EffectState:: 
  hasEffectByWithRemove<BLACK>(Square, Square) const;
  template bool EffectState:: 
  hasEffectByWithRemove<WHITE>(Square, Square) const;
  template void EffectState::makeKing8Info<BLACK>();
  template void EffectState::makeKing8Info<WHITE>();

#ifndef DFPNSTATONE
  template Piece 
  EffectState::safeCaptureNotByKing<BLACK>(Square, Piece) const;
  template Piece 
  EffectState::safeCaptureNotByKing<WHITE>(Square, Piece) const;
#endif
}

// kingOpenMove.cc
template <osl::Player P>
bool osl::move_classifier::KingOpenMove::
isMember(const EffectState& state, Ptype, Square from, Square to,
         Square exceptFor)
{
  assert(! from.isPieceStand());
  Square king_position=state.kingSquare<P>();
  if (king_position.isPieceStand())
    return false;
  /**
   * 守っている玉が動く状況では呼ばない
   */
  assert(king_position != from);
  /**
   * openになってしまうかどうかのチェック
   */
  Offset offset=base8_step(king_position,from);
  /**
   * 移動元が王の8方向でないか
   * openにならない
   */
  if(offset == Offset_ZERO
     || offset==base8_step(king_position, to))
    return false;
  if(!state.isEmptyBetween(from,king_position,offset,true)) return false;
  Square pos=from;
  Piece p;
  for(pos-=offset;;pos-=offset){
    // TODO: exceptFor を毎回チェックする必要があるのはoffset方向の時だけ
    if (! ((pos == exceptFor) || (p=state.pieceAt(pos), p.isEmpty())))
      break;
    assert(pos.isOnBoard());
  }
  /**
   * そのptypeoがそのoffsetを動きとして持つか
   * 注: 持つ => safe でない => false を返す
   */
  if (! p.isOnBoardByOwner<alt(P)>())
    return false;
  return any(ptype_effect(p.ptypeO(),pos,king_position));
}

// init order
// tables.cc
namespace
{
  bool canCheckmate(osl::Ptype ptype,osl::Direction dir,unsigned int mask)
  {
    // 王はdropできない, 打ち歩詰め
    if(ptype==osl::KING || ptype==osl::PAWN) return false;
    // ptypeがdir方向に利きを持たない == 王手をかけられない
    if(!(osl::ptype_move_direction[Int(ptype)]
         & (one_hot(dir) | one_hot(to_long(dir)))))
      return false;
    int dx=black_dx(dir), dy=black_dy(dir);
    for (auto dir1: osl::base8_directions()) {
      if (!bittest(mask, dir1)) continue;
      int dx1=black_dx(dir1), dy1=black_dy(dir1);
      auto o32 = osl::to_offset32(dx-dx1,dy-dy1);
      if(! any(ptype_effect(newPtypeO(osl::BLACK,ptype),o32)))
	return false;
    }
    return true;
  }
}

osl::checkmate::ImmediateCheckmateTable::ImmediateCheckmateTable()
{
  // ptypeDropMaskの初期化
  for(int i=0;i<0x100;i++){
    for(auto ptype: all_basic_ptype()) {
      unsigned char mask=0;
      for (auto dir: base8_directions()) {
	// 玉の逃げ道がある
	if (bittest(i, dir)) continue;
	if(canCheckmate(ptype,dir,i))
	  mask |= one_hot(dir);
      }
      ptypeDropMasks[i][idx(ptype)]=mask;
    }
  }
  // dropPtypeMaskの初期化
  for(int i=0;i<0x10000;i++){
    unsigned char ptypeMask=0;
    for (Ptype ptype: all_basic_ptype()) {
      for (auto dir: base8_directions()){
	// 空白でない
	if (!bittest(i, dir)) continue;
	// 玉の逃げ道がある
	if((i&(0x100<<idx(dir)))!=0)continue;
	if(canCheckmate(ptype,dir,(i>>8)&0xff)){
	  ptypeMask |= 1u<<basic_idx(ptype);
	  goto nextPtype;
	}
      }
    nextPtype:;
    }
    dropPtypeMasks[i]=ptypeMask;
  }
  // blockingMaskの初期化
  for (auto ptype: all_basic_ptype()) {
    for (auto dir: base8_directions()){
      unsigned int mask=0;
      if(ptype_move_direction[Int(ptype)]
         & (one_hot(dir) | one_hot(to_long(dir)))){
	int dx=black_dx(dir);
	int dy=black_dy(dir);
	for (auto dir1: base8_directions()) {
	  int dx1=black_dx(dir1);
	  int dy1=black_dy(dir1);
	  auto  o32 = osl::to_offset32(dx-dx1,dy-dy1);
	  if(! any(ptype_effect(newPtypeO(BLACK,ptype),o32))){
	    if(base8_step(o32) != Offset_ZERO && !(dx==-dx1 && dy==-dy1)){
	      mask |= one_hot(dir1);
	    }
	  }
	}
      }
      blockingMasks[idx(ptype)][dir]=mask;
    }
  }
  // effectMaskの初期化
  for (auto ptype: all_piece_ptype()) {
    for (auto dir: base8_directions()) {
      unsigned int mask=0x1ff;
      if(ptype_move_direction[Int(ptype)]
         & (one_hot(dir) | one_hot(to_long(dir)))){ // 王手をかけられる
	mask=0;
	int dx=black_dx(dir);
	int dy=black_dy(dir);
	for (auto dir1: base8_directions()) {
	  int dx1=black_dx(dir1);
	  int dy1=black_dy(dir1);
	  auto o32 = to_offset32(dx-dx1,dy-dy1);
	  if(dir!= dir1 &&
	     ! any(ptype_effect(newPtypeO(BLACK,ptype),o32))){
	    mask |= one_hot(dir1);
	  }
	}
      }
      noEffectMasks[idx(ptype)][dir]=mask;
    }
  }
}


namespace osl {
  // table generators
  namespace {
    // boardTable.cc
    auto make_Long_Directions() {
      CArray<Direction,Offset32_SIZE> table;
      table.fill(Direction());  // Direction(0) is suitable for sentinel as others are is_long.
      for (auto dir: long_directions()) {
        const int dx=black_dx(dir), dy=black_dy(dir);
        for (int n: std::views::iota(1,9))
          table[idx(to_offset32(dx*n, dy*n))] = dir;
      }
      return table;
    }
    auto make_Basic10_Offsets() {
      CArray<Offset, Offset32_SIZE> table;
      table.fill(Offset_ZERO);
      for (auto dir: long_directions()) {
        const int dx=black_dx(dir), dy=black_dy(dir);
        Offset offset=make_offset(dx,dy);
        for (int n: std::views::iota(1,9)) 
          table[idx(to_offset32(n*dx, n*dy))]= offset;
      }
      for (auto dir: knight_directions) {
        int dx=black_dx(dir), dy=black_dy(dir);
        Offset32 offset32=to_offset32(dx,dy);
        Offset offset=make_offset(dx,dy);
        table[idx(offset32)]=offset;
        table[idx(-offset32)]= -offset;
      }
      return table;
    }
    auto make_Base8_Offsets_Rich() {
      CArray<Offset, Offset32_SIZE> table;
      table.fill(Offset_ZERO);
      for (auto dir: long_directions()) {
        const int dx=black_dx(dir), dy=black_dy(dir);
        Offset offset=make_offset(dx,dy);
        for (int n: std::views::iota(1,9))
          table[idx(to_offset32(n*dx, n*dy))]= offset;
      }
      return table;
    }

    auto make_Base8Offset() {
      CArray<signed char, OnBoard_Offset_SIZE> table;
      table.fill(0);
      for (auto dir: long_directions()) {
        const int dx=black_dx(dir), dy=black_dy(dir);
        Offset offset=make_offset(dx,dy);
        for (int n: std::views::iota(1,9))
          table[onboard_idx(make_offset(n*dx,n*dy))]= Int(offset);
      }
      return table;
    }
    auto make_Base8Direction() {
      CArray<unsigned char, OnBoard_Offset_SIZE> table;
      table.fill(Direction_INVALID_VALUE);
      for (auto dir: long_directions()) {
        const int dx=black_dx(dir), dy=black_dy(dir);
        for (int n: std::views::iota(1,9))
          table[onboard_idx(make_offset(n*dx,n*dy))]= Int(long_to_base8(dir));
      }
      return table;
    }
    static_assert(sizeof(EffectDirection) == 4, "size");
    // ptypeTable
    auto make_Effect_Table() {
      CArray2d<EffectDirection,Ptypeo_SIZE,Offset32_SIZE> table;
      table.fill();
      for(auto ptype: all_ptype_range()) {
        for (auto dir: all_directions()) {          
          if(bittest(ptype_move_direction[idx(ptype)], dir)){
            int dx=black_dx(dir), dy=black_dy(dir), pb=Int(ptype)-Ptypeo_MIN, pw=Int(ptype)-16-Ptypeo_MIN;
            Offset32 offset32=to_offset32(dx,dy);
            Offset offset = make_offset(dx,dy);
            if(is_long(dir)) {
              table[pb][idx(offset32)] = pack_long_neighbor(offset);
              table[pw][idx(-offset32)]= pack_long_neighbor(-offset);

              for(int i=2;i<9;i++){
                offset32=to_offset32(dx*i,dy*i);
                table[pb][idx(offset32)] = pack_long_far(offset);
                table[pw][idx(-offset32)]= pack_long_far(-offset);
              }
            }
            else{
              table[pb][idx(offset32)] = EffectDefinite;
              table[pw][idx(-offset32)]= EffectDefinite;
            }
          }
        }
      }
      return table;
    }
  }

  auto make_BoardMaskTable3x3() {
    CArray<BoardMask, Square::SIZE> table;
    for (int cy: board_y_range()) {
      for (int cx: board_x_range()) {
        const int min_x = std::max(1, cx - 1), max_x = std::min(9, cx + 1);
        const int min_y = std::max(1, cy - 1), max_y = std::min(9, cy + 1);
        BoardMask mask;
        mask.clear();
        for (int x: std::views::iota(min_x, max_x+1))
          for (int y: std::views::iota(min_y, max_y+1))
            mask.set(Square(x,y));
        table[Square(cx,cy)] = mask;
      }
    }
    return table;
  }
}


namespace osl
{
  // NOTE: order matters here
  const CArray<Direction,Offset32_SIZE> board::Long_Directions = make_Long_Directions();
  const CArray<Offset, Offset32_SIZE> board::Basic10_Offsets = make_Basic10_Offsets();
  const CArray<Offset, Offset32_SIZE> board::Base8_Offsets_Rich = make_Base8_Offsets_Rich();
  const CArray<signed char, OnBoard_Offset_SIZE> board::Base8_Offsets = make_Base8Offset();
  const CArray<unsigned char, OnBoard_Offset_SIZE> board::Base8_Directions = make_Base8Direction();
  // PtypeTable depends on arrays above from ex Board_Table
  const CArray2d<EffectDirection,Ptypeo_SIZE,Offset32_SIZE> board::Ptype_Effect_Table = make_Effect_Table();

  const checkmate::ImmediateCheckmateTable checkmate::Immediate_Checkmate_Table;
  const CArray<BoardMask, Square::SIZE> BoardMaskTable3x3 = make_BoardMaskTable3x3();
}

