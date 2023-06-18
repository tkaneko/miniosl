#include "impl/effect.h"
#include "checkmate.h"
#include <iostream>
#include <iomanip>
#include <bitset>

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

namespace osl
{
  // explicit template instantiation
  template void EffectSummary::doEffect<BLACK,EffectAdd>(BaseState const&, PtypeO, Square, int);
  template void EffectSummary::doEffect<BLACK,EffectSub>(BaseState const&, PtypeO, Square, int);
  template void EffectSummary::doEffect<WHITE,EffectAdd>(BaseState const&, PtypeO, Square, int);
  template void EffectSummary::doEffect<WHITE,EffectSub>(BaseState const&, PtypeO, Square, int);

  template void EffectSummary::doBlockAt<EffectAdd>(BaseState const&, Square, int);
  template void EffectSummary::doBlockAt<EffectSub>(BaseState const&, Square, int);
}

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
