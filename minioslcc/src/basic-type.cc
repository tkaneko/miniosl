#include "state.h"
#include "record.h"
#include <iostream>

std::ostream& osl::operator<<(std::ostream& os, Player player) {
  return os << ("+-"[idx(player)]);
}
std::ostream& osl::operator<<(std::ostream& os, Ptype ptype) {
  return os << ptype_en_names[Int(ptype)];
}
std::istream& osl::operator>>(std::istream& is, Ptype& ptype) {
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
std::ostream& osl::operator<<(std::ostream& os, PtypeO ptypeO) {
  if (is_piece(ptypeO))
    return os << "PtypeO(" << owner(ptypeO) << "," 
	      << ptype(ptypeO) << ")";
  return os << "PtypeO(" << (int)ptypeO << "," << ptype(ptypeO) << ")";
}
std::ostream& osl::operator<<(std::ostream& os, Direction d){
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
  if (move.isSpecial())
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
      os << "Move(" << turn
	 << to_csa(move.from()) << to_csa(move.to())
         << to_csa(move.ptype());
      if (move.promoteMask())
	os << "+";
      if (capture_ptype != Ptype_EMPTY)
	os << "x" << to_csa(capture_ptype);
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


