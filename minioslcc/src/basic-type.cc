#include "state.h"
#include "record.h"
#include "more.h"
#include <iostream>

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
    return os << "piece-stand";
  return os << "Square(" << square.x() << ',' << square.y() << ")";
}

static_assert(sizeof(osl::Piece) == 4, "piece size");

std::ostream& osl::operator<<(std::ostream& os, Piece piece) {
  if (piece.isPiece())
    os << "Piece(" << piece.owner() << ", " << piece.ptype() << ", " << piece.square()
       << ", num=" << piece.id() << ')';
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

bool osl::Move::is_ordinary_valid() const {
  if (! isNormal())
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

osl::Move osl::Move::rotate180() const {
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
