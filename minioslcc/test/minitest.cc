#include "acutest.h"
#include "state.h"
#include "more.h"
#include "record.h"
#include "checkmate.h"
#include <iostream>
#include <bitset>
#include <algorithm>

#define TEST_CHECK_EQUAL(a,b) TEST_CHECK((a) == (b))
#define TEST_ASSERT_EQUAL(a,b) TEST_ASSERT((a) == (b))

using namespace osl;

template <class Container, class T>
bool is_member(const Container& c, const T& val) {
  return std::find(c.begin(), c.end(), val) != c.end();
}

template <class T>
std::string make_string_rep(T val) {
  std::ostringstream ss;
  ss << val;
  return ss.str();
}

void test_player() {
  TEST_CHECK(alt(BLACK)==WHITE);
  TEST_CHECK(alt(WHITE)==BLACK);

  TEST_CHECK_EQUAL(make_string_rep(BLACK), "+");
  TEST_CHECK_EQUAL(make_string_rep(WHITE), "-");
}

void test_ptype() {
  static_assert(sizeof(all_ptype) == (Ptype_SIZE)*sizeof(Ptype));
  static_assert(sizeof(piece_ptype) == (Ptype_SIZE-Ptype_Piece_MIN)*sizeof(Ptype));

  TEST_CHECK_EQUAL(false, (legal_drop_at(BLACK,PAWN, Square(4,1))));

  TEST_CHECK(is_promoted(PPAWN) == true);
  TEST_CHECK(is_promoted(PROOK) == true);
  TEST_CHECK(is_promoted(KING) == false);
  TEST_CHECK(is_promoted(ROOK) == false);
  
  for (auto ptype: piece_ptype) {
    if (is_promoted(ptype) || ptype==GOLD || ptype==KING) {
      TEST_CHECK(can_promote(ptype) == false);
    }
    else {
      TEST_CHECK(can_promote(ptype) == true);
    }
  }

  TEST_CHECK(promote(PAWN) == PPAWN);
  TEST_CHECK(promote(ROOK) == PROOK);

  TEST_CHECK(unpromote(PPAWN) == PAWN);
  TEST_CHECK(unpromote(PROOK) == ROOK);
  TEST_CHECK(unpromote(KING) == KING);
  TEST_CHECK(unpromote(ROOK) == ROOK);

  for (int i=0;i<2;i++) {
    Player pl=static_cast<Player>(-i);
    for (auto ptype: piece_ptype) {
      PtypeO ptypeO=newPtypeO(pl,ptype);
      PtypeO altPtypeO=newPtypeO(alt(pl),ptype);
      TEST_CHECK_EQUAL(altPtypeO,alt(ptypeO));
    }
  }

  for (int i=0;i<2;i++) {
    Player pl=static_cast<Player>(-i);
    for (auto ptype: piece_ptype) {
      PtypeO ptypeO=newPtypeO(pl,ptype);
      PtypeO altPtypeO=newPtypeO(alt(pl),ptype);
      TEST_CHECK_EQUAL(altPtypeO, alt(ptypeO));
    }
  }
  TEST_CHECK_EQUAL(Ptypeo_EMPTY, alt(Ptypeo_EMPTY));
  TEST_CHECK_EQUAL(Ptypeo_EDGE, alt(Ptypeo_EDGE));

  for (auto ptype: piece_ptype) {
    if (can_promote(ptype)) {
      TEST_CHECK(unpromote(promote(ptype)) == ptype);
    }
  }

  TEST_CHECK(ptype_has_long_move(LANCE));
  TEST_CHECK(ptype_has_long_move(ROOK));

  TEST_CHECK_EQUAL(piece_id_set(PAWN), 0x3ffffuLL);
  TEST_CHECK_EQUAL(piece_id_set(ROOK), 0xc000000000uLL);

  {
    int dx= -1, dy=8;
    Ptype ptype=ROOK;
    const auto effect=ptype_effect(newPtypeO(BLACK,ptype),to_offset32(dx,dy));
    TEST_CHECK(! has_long(effect));
    TEST_CHECK(! is_definite(effect));
  
    TEST_CHECK(! any(ptype_effect(newPtypeO(BLACK,ROOK),to_offset32(-1,8))));
  }

  for (auto ptype: all_ptype_range()) {
    if (is_piece(ptype))
    {
      TEST_CHECK_EQUAL((unpromote(ptype) == ROOK 
			    || unpromote(ptype) == BISHOP),
			   is_major(ptype));
    }
    TEST_CHECK_EQUAL((ptype == ROOK || ptype == BISHOP),
			 is_major_basic(ptype));
  }  

  {
    TEST_CHECK_EQUAL(ptype_effect(newPtypeO(BLACK,PPAWN),to_offset32(Square(7,1),Square(8,1))),
                     EffectDefinite);
  }
  {
    TEST_CHECK(! any(ptype_effect(newPtypeO(BLACK,ROOK), to_offset32(-1,8))));
  }
}

void test_direction() {  
  TEST_CHECK_EQUAL(DR,inverse(UL));
  TEST_CHECK_EQUAL(D,inverse(U));
  TEST_CHECK_EQUAL(DL,inverse(UR));
  TEST_CHECK_EQUAL(R,inverse(L));
  TEST_CHECK_EQUAL(L,inverse(R));
  TEST_CHECK_EQUAL(UR,inverse(DL));
  TEST_CHECK_EQUAL(U,inverse(D));
  TEST_CHECK_EQUAL(UL,inverse(DR));

  TEST_CHECK_EQUAL(UL,primary(UL));
  TEST_CHECK_EQUAL(U,primary(U));
  TEST_CHECK_EQUAL(UR,primary(UR));
  TEST_CHECK_EQUAL(L,primary(L));
  TEST_CHECK_EQUAL(L,primary(R));
  TEST_CHECK_EQUAL(UR,primary(DL));
  TEST_CHECK_EQUAL(U,primary(D));
  TEST_CHECK_EQUAL(UL,primary(DR));

  TEST_CHECK(black_dx(Long_D)==0);
  TEST_CHECK(black_dy(Long_D)==1);
  TEST_CHECK(black_offset(Long_D)==make_offset(0,1));
  TEST_CHECK(to_offset(BLACK,Long_D)==make_offset(0,1));
  TEST_CHECK(to_offset(BLACK,Long_D)!=Offset_ZERO);
  TEST_CHECK(to_offset(WHITE,UUR)==make_offset(1,2));
  TEST_CHECK(to_offset(WHITE,UUR)==Offset(18));

  TEST_CHECK_EQUAL(basic_step(to_offset32(6,0)), make_offset(1,0));
  TEST_CHECK(black_dy(Long_D)==1);
  TEST_CHECK(black_offset(Long_D)==make_offset(0,1));
  TEST_CHECK(to_offset(BLACK,Long_D)==make_offset(0,1));
  TEST_CHECK(to_offset(BLACK,Long_D)!=Offset_ZERO);
  for (int x0: board_x_range())
    for (int y0: board_y_range()) {
      Square pos0(x0,y0);
      for (int x1: board_x_range())
	for (int y1: board_y_range()) {
	  Square pos1(x1,y1);
	  if (x0==x1 && y0==y1) continue;
	  if (x0==x1 || y0==y1 || abs(x0-x1)==abs(y0-y1)) {
	    TEST_CHECK_EQUAL(long_to_base8(to_long_direction<BLACK>(pos0,pos1)),
                             base8_dir<BLACK>(pos0,pos1));
	  }
	}
    }
}

static bool in_good_range(Square pos) {
  return (pos.index() > 0) && (pos.index() < Square::indexMax());
}
void test_square() {
  TEST_CHECK(in_good_range(Square(1,1)));
  TEST_CHECK(in_good_range(Square(0,0)));
  // 24近傍が配列におさまると楽なこともあるけど，現状は違うようだ
  // TEST_CHECK(! can_handle(Square(-1,-1)));

  for(int x=9;x>=1;x--)
    for(int y=1;y<=9;y++){
      Square pos(x,y);
      TEST_CHECK(pos.x() == x);
      TEST_CHECK(pos.y() == y);
    }

  TEST_CHECK(Square::STAND().isOnBoard() ==false);
  for(int i=-(16*16);i<Square::SIZE+(16*16);i++)
    {
    Square pos=Square::nth((unsigned int)i);
    if(i>=0 && i<256 && 1<=pos.x() && pos.x()<=9 && 1<=pos.y() && pos.y()<=9){
      TEST_CHECK(pos.isOnBoard() == true);
    }
    else{
      TEST_CHECK(pos.isOnBoard() == false);
    }
  }
  TEST_CHECK(Square(0, 4).isOnBoard() ==false);
  TEST_CHECK(Square(10,5).isOnBoard() ==false);
  TEST_CHECK(Square(4, 0).isOnBoard() ==false);
  TEST_CHECK(Square(5,10).isOnBoard() ==false);
  for(int x=0;x<=10;x++)
    for(int y=0;y<=10;y++){
      Square pos(x,y);
      if(x==0 || x==10 || y==0 || y==10){
	if(!pos.isEdge()){
	  std::cerr << "position=" << pos << std::endl;
	}
	TEST_CHECK(pos.isEdge());
      }
      else{
	if(pos.isEdge()){
	  std::cerr << "position=" << pos << std::endl;
	}
	TEST_CHECK(!pos.isEdge());
      }
    }
  TEST_CHECK(Square(0, 4).isEdge());
  TEST_CHECK(Square(10,5).isEdge());
  TEST_CHECK(Square(4, 0).isEdge());
  TEST_CHECK(Square(5,10).isEdge());

  TEST_CHECK(Square::STAND().isValid() ==true);
  for(int x=9;x>0;x--)
    for(int y=1;y<=9;y++){
      Square pos(x,y);
      TEST_CHECK(pos.isValid() == true);
    }
  TEST_CHECK(Square(0, 4).isValid() ==false);
  TEST_CHECK(Square(10,5).isValid() ==false);
  TEST_CHECK(Square(4, 0).isValid() ==false);
  TEST_CHECK(Square(5,10).isValid() ==false);

  TEST_CHECK(Square(9,9)-Square(7,6)==make_offset(2,3));
  TEST_CHECK(Square(6,7)-Square(9,9)==make_offset(-3,-2));

  TEST_CHECK(Square(9,9)-make_offset(2,3)==Square(7,6));
  TEST_CHECK(Square(6,7)+make_offset(-3,-2)==Square(3,5));
  Square p(6,7);
  p+=make_offset(-2,2);
  TEST_CHECK(p==Square(4,9));
  p-=make_offset(-5,3);
  TEST_CHECK(p==Square(9,6));
}

bool max_of_two(PieceStand l, PieceStand r, PieceStand m) {
  for (auto ptype: PieceStand::order) {
    if (std::max(l.get(ptype), r.get(ptype)) != m.get(ptype))
      return false;
  }
  return true;
}

void test_piece_stand() {
  {
    const PieceStand l(12,1,2,3,4,1,0,0);
    const PieceStand r(8,3,0,4,2,0,2,0);
    const PieceStand m = l.max(r);
    TEST_CHECK(max_of_two(l, r, m));
  }
  {
    unsigned int flag=0;
    flag |= (1<<30);
    flag |= (1<<27);
    flag |= (1<<23);
    flag |= (1<<17);
    flag |= (1<<13);
    flag |= (1<<9);
    flag |= (1<<5);
    flag |= (1<<2);
    TEST_CHECK_EQUAL(flag, PieceStand::carryMask);
  }
  {
    PieceStand pieces;
    pieces.carriesOn();
    for (auto ptype: basic_ptype)
      TEST_CHECK_EQUAL(0u, pieces.get(ptype));
    pieces.carriesOff();
    for (auto ptype: basic_ptype)
      TEST_CHECK_EQUAL(0u, pieces.get(ptype));
  }
  {
    PieceStand pieces;
    for (auto pi: basic_ptype) {
      const unsigned int num =	// 持駒になる最大の個数
        ptype_piece_id[Int(pi)].second - ptype_piece_id[Int(pi)].first;
      TEST_CHECK_EQUAL(0u, pieces.get(pi));
      pieces.add(pi, num);
      for (auto pj: basic_ptype) {
        if (pi==pj)
          TEST_CHECK_EQUAL(num, pieces.get(pj));
        else
          TEST_CHECK_EQUAL(0u, pieces.get(pj));
      }
      pieces.sub(pi, num);
      TEST_CHECK_EQUAL(0u, pieces.get(pi));
    }
  }
  {
    PieceStand p1, p2;
    TEST_CHECK(p1.isSuperiorOrEqualTo(p2));
    TEST_CHECK(p2.isSuperiorOrEqualTo(p1));
  
    p1.add(PAWN, 3);

    TEST_CHECK(p1.isSuperiorOrEqualTo(p2));
    TEST_CHECK(! p2.isSuperiorOrEqualTo(p1));

    TEST_CHECK_EQUAL(3u, p1.get(PAWN));

    p2.add(GOLD, 1);
  
    TEST_CHECK(! p1.isSuperiorOrEqualTo(p2));
    TEST_CHECK(! p2.isSuperiorOrEqualTo(p1));

    p1.add(GOLD, 1);

    TEST_CHECK(p1.isSuperiorOrEqualTo(p2));
    TEST_CHECK(! p2.isSuperiorOrEqualTo(p1));
  }
  {
    PieceStand p1;
    PieceStand   pPawn(1,0,0,0,0,0,0,0);
    PieceStand  pLance(0,1,0,0,0,0,0,0);
    PieceStand pKnight(0,0,1,0,0,0,0,0);
    PieceStand pSilver(0,0,0,1,0,0,0,0);
    PieceStand   pGold(0,0,0,0,1,0,0,0);
    PieceStand pBishop(0,0,0,0,0,1,0,0);
    PieceStand   pRook(0,0,0,0,0,0,1,0);
    PieceStand   pKing(0,0,0,0,0,0,0,1);

    p1.addAtmostOnePiece(pPawn);
    p1.addAtmostOnePiece(pKnight);
    p1.addAtmostOnePiece(pGold);
    p1.addAtmostOnePiece(pRook);

    TEST_CHECK_EQUAL(PieceStand(1,0,1,0,1,0,1,0),p1);

    p1.addAtmostOnePiece(pPawn);
    p1.addAtmostOnePiece(pLance);
    p1.addAtmostOnePiece(pKnight);
    p1.addAtmostOnePiece(pSilver);

    TEST_CHECK_EQUAL(PieceStand(2,1,2,1,1,0,1,0),p1);

    p1.addAtmostOnePiece(pPawn);
    p1.addAtmostOnePiece(pLance);
    p1.addAtmostOnePiece(pGold);
    p1.addAtmostOnePiece(pBishop);

    TEST_CHECK_EQUAL(PieceStand(3,2,2,1,2,1,1,0),p1);

    p1.addAtmostOnePiece(pPawn);
    p1.addAtmostOnePiece(pLance);
    p1.addAtmostOnePiece(pKnight);
    p1.addAtmostOnePiece(pSilver);
    p1.addAtmostOnePiece(pGold);
    p1.addAtmostOnePiece(pBishop);
    p1.addAtmostOnePiece(pRook);
    p1.addAtmostOnePiece(pKing);

    TEST_CHECK_EQUAL(PieceStand(4,3,3,2,3,2,2,1),p1);

    p1.subAtmostOnePiece(pPawn);
    p1.subAtmostOnePiece(pKnight);
    p1.subAtmostOnePiece(pGold);
    p1.subAtmostOnePiece(pRook);

    TEST_CHECK_EQUAL(PieceStand(3,3,2,2,2,2,1,1),p1);

    p1.subAtmostOnePiece(pPawn);
    p1.subAtmostOnePiece(pLance);
    p1.subAtmostOnePiece(pKnight);
    p1.subAtmostOnePiece(pSilver);

    TEST_CHECK_EQUAL(PieceStand(2,2,1,1,2,2,1,1),p1);

    p1.subAtmostOnePiece(pPawn);
    p1.subAtmostOnePiece(pLance);
    p1.subAtmostOnePiece(pGold);
    p1.subAtmostOnePiece(pBishop);

    TEST_CHECK_EQUAL(PieceStand(1,1,1,1,1,1,1,1),p1);

    p1.subAtmostOnePiece(pPawn);
    p1.subAtmostOnePiece(pLance);
    p1.subAtmostOnePiece(pKnight);
    p1.subAtmostOnePiece(pSilver);
    p1.subAtmostOnePiece(pGold);
    p1.subAtmostOnePiece(pBishop);
    p1.subAtmostOnePiece(pRook);
    p1.subAtmostOnePiece(pKing);

    TEST_CHECK_EQUAL(PieceStand(0,0,0,0,0,0,0,0),p1);
  }
  {
    PieceStand src;
    for (auto ptype: PieceStand::order) {
        src.add(ptype, random() % 3);
    }
    std::stringstream ss;
    PieceStandIO::writeNumbers(ss, src);
    PieceStand dst;
    PieceStandIO::readNumbers(ss, dst);
    
    TEST_CHECK_EQUAL(src, dst);
  }
}

void test_move() {
  {
    const Move drop(Square(5,5), KNIGHT, WHITE);
    TEST_CHECK(drop.isValid()); 

    const Move bug = Move::makeDirect(-58698240);
    TEST_CHECK(! bug.isValid()); // KINGを取る55桂打
  }
  {
    const Move m76fu(Square(7,7),Square(7,6),PAWN,Ptype_EMPTY,false,BLACK);
    TEST_CHECK(! m76fu.isPass());
    TEST_CHECK(m76fu.isNormal());
    TEST_CHECK(! m76fu.isInvalid());

    const Move m34fu(Square(3,3),Square(3,4),PAWN,Ptype_EMPTY,false,WHITE);
    TEST_CHECK(! m34fu.isPass());
    TEST_CHECK(m34fu.isNormal());
    TEST_CHECK(! m34fu.isInvalid());
  }
  {
    const Move pass_black = Move::PASS(BLACK);
    TEST_CHECK_EQUAL(Ptype_EMPTY, pass_black.ptype());
    TEST_CHECK_EQUAL(Ptype_EMPTY, pass_black.oldPtype());
    TEST_CHECK_EQUAL(Square::STAND(), pass_black.from());
    TEST_CHECK_EQUAL(Square::STAND(), pass_black.to());
    TEST_CHECK_EQUAL(BLACK, pass_black.player());

    TEST_CHECK(pass_black.isPass());
    TEST_CHECK(! pass_black.isNormal());
    TEST_CHECK(! pass_black.isInvalid());

    const Move pass_white = Move::PASS(WHITE);
    TEST_CHECK_EQUAL(Ptype_EMPTY, pass_white.ptype());
    TEST_CHECK_EQUAL(Ptype_EMPTY, pass_white.oldPtype());
    TEST_CHECK_EQUAL(Square::STAND(), pass_white.from());
    TEST_CHECK_EQUAL(Square::STAND(), pass_white.to());
    TEST_CHECK_EQUAL(WHITE, pass_white.player());

    TEST_CHECK(pass_white.isPass());
    TEST_CHECK(! pass_white.isNormal());
    TEST_CHECK(! pass_white.isInvalid());
  }
  {
    TEST_CHECK(Move::DeclareWin().isInvalid());
    TEST_CHECK(! Move::DeclareWin().isNormal());
  }
  {
    const Square from(7,7);
    const Square to(7,6);
    const Ptype ptype = GOLD;
    const Player player = BLACK;
    const Ptype capture_ptype = Ptype_EMPTY;
    const bool promote = false;
    Move m(from, to, ptype, capture_ptype, promote, player);
    Move m_copy(m);
#if 0
    m=m.newFrom(Square(8,7));
    TEST_CHECK_EQUAL(Square(8,7), m.from());
    TEST_CHECK_EQUAL(to, m.to());
    TEST_CHECK_EQUAL(ptype, m.ptype());
    TEST_CHECK_EQUAL(capture_ptype, m.capturePtype());
    TEST_CHECK_EQUAL(promote, m.isPromotion());
    TEST_CHECK_EQUAL(player, m.player());

    m=m.newFrom(Square(7,7));
    TEST_CHECK_EQUAL(m_copy, m);
#endif
  }
  {
    const Move drop_b(Square(5,5), KNIGHT, BLACK);
    const Move drop_w(Square(5,5), KNIGHT, WHITE);
    TEST_CHECK_EQUAL(Ptypeo_EMPTY, drop_w.capturePtypeOSafe());
    TEST_CHECK_EQUAL(Ptypeo_EMPTY, drop_b.capturePtypeOSafe());
  }
  {
    // pawn
    TEST_CHECK(Move(Square(4,4),Square(4,3),PAWN,Ptype_EMPTY,false,BLACK).ignoreUnpromote());
    TEST_CHECK(!Move(Square(4,4),Square(4,3),PPAWN,Ptype_EMPTY,true,BLACK).ignoreUnpromote());
    TEST_CHECK(!Move(Square(4,3),PAWN,BLACK).ignoreUnpromote());
    TEST_CHECK(!Move(Square(4,5),Square(4,4),PAWN,Ptype_EMPTY,false,BLACK).ignoreUnpromote());
    TEST_CHECK(Move(Square(4,6),Square(4,7),PAWN,Ptype_EMPTY,false,WHITE).ignoreUnpromote());
    TEST_CHECK(!Move(Square(4,6),Square(4,7),PPAWN,Ptype_EMPTY,true,WHITE).ignoreUnpromote());
    TEST_CHECK(!Move(Square(4,7),PAWN,WHITE).ignoreUnpromote());
    TEST_CHECK(!Move(Square(4,5),Square(4,6),PAWN,Ptype_EMPTY,false,WHITE).ignoreUnpromote());
    // lance
    TEST_CHECK(!Move(Square(4,4),Square(4,3),LANCE,Ptype_EMPTY,false,BLACK).ignoreUnpromote());
    TEST_CHECK(Move(Square(4,4),Square(4,2),LANCE,Ptype_EMPTY,false,BLACK).ignoreUnpromote());
    TEST_CHECK(!Move(Square(4,4),Square(4,3),PLANCE,Ptype_EMPTY,true,BLACK).ignoreUnpromote());
    TEST_CHECK(!Move(Square(4,3),LANCE,BLACK).ignoreUnpromote());
    TEST_CHECK(!Move(Square(4,5),Square(4,4),LANCE,Ptype_EMPTY,false,BLACK).ignoreUnpromote());
    TEST_CHECK(!Move(Square(4,6),Square(4,7),LANCE,Ptype_EMPTY,false,WHITE).ignoreUnpromote());
    TEST_CHECK(Move(Square(4,6),Square(4,8),LANCE,Ptype_EMPTY,false,WHITE).ignoreUnpromote());
    TEST_CHECK(!Move(Square(4,6),Square(4,7),PLANCE,Ptype_EMPTY,true,WHITE).ignoreUnpromote());
    TEST_CHECK(!Move(Square(4,7),LANCE,WHITE).ignoreUnpromote());
    TEST_CHECK(!Move(Square(4,5),Square(4,6),LANCE,Ptype_EMPTY,false,WHITE).ignoreUnpromote());
    // bishop
    TEST_CHECK(Move(Square(4,4),Square(2,2),BISHOP,Ptype_EMPTY,false,BLACK).ignoreUnpromote());
    TEST_CHECK(Move(Square(4,2),Square(8,6),BISHOP,Ptype_EMPTY,false,BLACK).ignoreUnpromote());
    TEST_CHECK(!Move(Square(4,4),Square(2,2),PBISHOP,Ptype_EMPTY,true,BLACK).ignoreUnpromote());
    TEST_CHECK(!Move(Square(4,2),Square(8,6),PBISHOP,Ptype_EMPTY,true,BLACK).ignoreUnpromote());
    TEST_CHECK(!Move(Square(4,3),BISHOP,BLACK).ignoreUnpromote());

    TEST_CHECK(Move(Square(6,6),Square(8,8),BISHOP,Ptype_EMPTY,false,WHITE).ignoreUnpromote());
    TEST_CHECK(Move(Square(6,8),Square(2,4),BISHOP,Ptype_EMPTY,false,WHITE).ignoreUnpromote());
    TEST_CHECK(!Move(Square(6,6),Square(8,8),PBISHOP,Ptype_EMPTY,true,WHITE).ignoreUnpromote());
    TEST_CHECK(!Move(Square(6,8),Square(2,4),PBISHOP,Ptype_EMPTY,true,WHITE).ignoreUnpromote());
    TEST_CHECK(!Move(Square(6,7),BISHOP,WHITE).ignoreUnpromote());
    // ROOK
    TEST_CHECK(Move(Square(4,4),Square(4,2),ROOK,Ptype_EMPTY,false,BLACK).ignoreUnpromote());
    TEST_CHECK(Move(Square(4,2),Square(4,6),ROOK,Ptype_EMPTY,false,BLACK).ignoreUnpromote());
    TEST_CHECK(!Move(Square(4,4),Square(4,2),PROOK,Ptype_EMPTY,true,BLACK).ignoreUnpromote());
    TEST_CHECK(!Move(Square(4,2),Square(4,6),PROOK,Ptype_EMPTY,true,BLACK).ignoreUnpromote());
    TEST_CHECK(!Move(Square(4,3),ROOK,BLACK).ignoreUnpromote());

    TEST_CHECK(Move(Square(6,6),Square(6,8),ROOK,Ptype_EMPTY,false,WHITE).ignoreUnpromote());
    TEST_CHECK(Move(Square(6,8),Square(6,4),ROOK,Ptype_EMPTY,false,WHITE).ignoreUnpromote());
    TEST_CHECK(!Move(Square(6,6),Square(6,8),PROOK,Ptype_EMPTY,true,WHITE).ignoreUnpromote());
    TEST_CHECK(!Move(Square(6,8),Square(6,4),PROOK,Ptype_EMPTY,true,WHITE).ignoreUnpromote());
    TEST_CHECK(!Move(Square(6,7),ROOK,WHITE).ignoreUnpromote());
  }
  {
    // pawn
    TEST_CHECK(Move(Square(4,4),Square(4,3),PPAWN,Ptype_EMPTY,true,BLACK).hasIgnoredUnpromote());
    TEST_CHECK(Move(Square(4,3),Square(4,2),PPAWN,Ptype_EMPTY,true,BLACK).hasIgnoredUnpromote());
    TEST_CHECK(!Move(Square(4,2),Square(4,1),PPAWN,Ptype_EMPTY,true,BLACK).hasIgnoredUnpromote());
    TEST_CHECK(!Move(Square(4,3),PAWN,BLACK).hasIgnoredUnpromote());
    TEST_CHECK(!Move(Square(4,5),Square(4,4),PAWN,Ptype_EMPTY,false,BLACK).hasIgnoredUnpromote());

    TEST_CHECK(Move(Square(4,6),Square(4,7),PPAWN,Ptype_EMPTY,true,WHITE).hasIgnoredUnpromote());
    TEST_CHECK(Move(Square(4,7),Square(4,8),PPAWN,Ptype_EMPTY,true,WHITE).hasIgnoredUnpromote());
    TEST_CHECK(!Move(Square(4,8),Square(4,9),PPAWN,Ptype_EMPTY,true,WHITE).hasIgnoredUnpromote());
    TEST_CHECK(!Move(Square(4,7),PAWN,WHITE).hasIgnoredUnpromote());
    TEST_CHECK(!Move(Square(4,5),Square(4,6),PAWN,Ptype_EMPTY,false,WHITE).hasIgnoredUnpromote());
    // lance
    TEST_CHECK(Move(Square(4,4),Square(4,2),PLANCE,Ptype_EMPTY,true,BLACK).hasIgnoredUnpromote());
    TEST_CHECK(!Move(Square(4,4),Square(4,3),PLANCE,Ptype_EMPTY,true,BLACK).hasIgnoredUnpromote());
    TEST_CHECK(!Move(Square(4,4),Square(4,1),PLANCE,Ptype_EMPTY,true,BLACK).hasIgnoredUnpromote());
    TEST_CHECK(!Move(Square(4,3),LANCE,BLACK).hasIgnoredUnpromote());
    TEST_CHECK(!Move(Square(4,5),Square(4,4),LANCE,Ptype_EMPTY,false,BLACK).hasIgnoredUnpromote());
    TEST_CHECK(Move(Square(4,6),Square(4,8),PLANCE,Ptype_EMPTY,true,WHITE).hasIgnoredUnpromote());
    TEST_CHECK(!Move(Square(4,6),Square(4,7),PLANCE,Ptype_EMPTY,true,WHITE).hasIgnoredUnpromote());
    TEST_CHECK(!Move(Square(4,6),Square(4,9),PLANCE,Ptype_EMPTY,true,WHITE).hasIgnoredUnpromote());
    TEST_CHECK(!Move(Square(4,7),LANCE,WHITE).hasIgnoredUnpromote());
    TEST_CHECK(!Move(Square(4,5),Square(4,6),LANCE,Ptype_EMPTY,false,WHITE).hasIgnoredUnpromote());
    // bishop
    TEST_CHECK(Move(Square(4,4),Square(2,2),PBISHOP,Ptype_EMPTY,true,BLACK).hasIgnoredUnpromote());
    TEST_CHECK(Move(Square(4,2),Square(8,6),PBISHOP,Ptype_EMPTY,true,BLACK).hasIgnoredUnpromote());
    TEST_CHECK(!Move(Square(4,4),Square(2,2),PBISHOP,Ptype_EMPTY,false,BLACK).hasIgnoredUnpromote());
    TEST_CHECK(!Move(Square(4,3),BISHOP,BLACK).hasIgnoredUnpromote());

    TEST_CHECK(Move(Square(6,6),Square(8,8),PBISHOP,Ptype_EMPTY,true,WHITE).hasIgnoredUnpromote());
    TEST_CHECK(Move(Square(6,8),Square(2,4),PBISHOP,Ptype_EMPTY,true,WHITE).hasIgnoredUnpromote());
    TEST_CHECK(!Move(Square(6,6),Square(8,8),PBISHOP,Ptype_EMPTY,false,WHITE).hasIgnoredUnpromote());
    TEST_CHECK(!Move(Square(6,7),BISHOP,WHITE).hasIgnoredUnpromote());
    // ROOK
    TEST_CHECK(Move(Square(4,4),Square(4,2),PROOK,Ptype_EMPTY,true,BLACK).hasIgnoredUnpromote());
    TEST_CHECK(Move(Square(4,2),Square(4,6),PROOK,Ptype_EMPTY,true,BLACK).hasIgnoredUnpromote());
    TEST_CHECK(!Move(Square(4,2),Square(4,6),PROOK,Ptype_EMPTY,false,BLACK).hasIgnoredUnpromote());
    TEST_CHECK(!Move(Square(4,3),ROOK,BLACK).hasIgnoredUnpromote());

    TEST_CHECK(Move(Square(6,6),Square(6,8),PROOK,Ptype_EMPTY,true,WHITE).hasIgnoredUnpromote());
    TEST_CHECK(Move(Square(6,8),Square(6,4),PROOK,Ptype_EMPTY,true,WHITE).hasIgnoredUnpromote());
    TEST_CHECK(!Move(Square(6,8),Square(6,4),PROOK,Ptype_EMPTY,false,WHITE).hasIgnoredUnpromote());
    TEST_CHECK(!Move(Square(6,7),ROOK,WHITE).hasIgnoredUnpromote());
  }
  {
    const Move m76(Square(7,7), Square(7,6), PAWN, Ptype_EMPTY, false, BLACK);
    const Move m34(Square(3,3), Square(3,4), PAWN, Ptype_EMPTY, false, WHITE);
    TEST_CHECK_EQUAL(m76.rotate180(), m34);

    const Move m41(Square(4,1), SILVER, BLACK);
    const Move m69(Square(6,9), SILVER, WHITE);
    TEST_CHECK_EQUAL(m41.rotate180(), m69);

    TEST_CHECK_EQUAL(Move::PASS(BLACK).rotate180(), Move::PASS(WHITE));
  }
}

void test_offset()
{
  TEST_CHECK(to_offset(BLACK, UL) != Offset_ZERO);
  for (int dx: std::views::iota(-8,9))
    for (int dy: std::views::iota(-8,9)) 
      for (auto ptypeo: all_ptypeO_range()) {
        auto offset32 = to_offset32(dx,dy);
        auto effect = ptype_effect(ptypeo, offset32);
        if (has_long(effect))
          TEST_CHECK_EQUAL(basic_step(offset32), to_offset(effect));
      }

  for (int x0: board_x_range())
    for (int y0: board_y_range()) {
      Square from(x0, y0);
      for (int x1: board_x_range())
        for (int y1: board_y_range()) {
          Square to(x1, y1);
          auto step = base8_step(to, from); // to-from v.s.
          if (step != Offset_ZERO) {
            auto [d, step8] = base8_dir_step<BLACK>(from, to); // from-to
            TEST_ASSERT(step == step8); // order ...
          }
        }
    }
}

void test_king8()
{
  {
    EffectState state(csa::read_board(
      "P1 *  *  *  * -OU *  *  *  * \n"
      "P2 *  *  *  *  *  *  *  *  * \n"
      "P3 *  *  *  * +FU *  *  *  * \n"
      "P4 *  *  *  *  *  *  *  *  * \n"
      "P5 *  *  *  *  *  *  *  *  * \n"
      "P6 *  *  *  *  *  *  *  *  * \n"
      "P7 *  *  *  * -FU *  *  *  * \n"
      "P8 *  *  *  *  *  *  *  *  * \n"
      "P9 *  *  *  * +OU *  *  *  * \n"
      "P+00KI\n"
      "P-00AL\n"
      "+\n"));
    King8Info black=state.king8Info(BLACK);
    King8Info white=state.king8Info(WHITE);
    // count
    TEST_CHECK_EQUAL(1, std::popcount(dropCandidate(black)));
    TEST_CHECK_EQUAL(1, std::popcount(dropCandidate(white)));
    // direction
    TEST_CHECK_EQUAL(one_hot(U), dropCandidate(black));
    TEST_CHECK_EQUAL(one_hot(U), dropCandidate(white));
  }
  {
    EffectState state(csa::read_board(
      "P1 *  *  *  * -OU *  *  *  * \n"
      "P2 *  *  *  *  *  *  *  *  * \n"
      "P3 *  *  *  *  * +FU *  *  * \n"
      "P4 *  *  *  *  *  *  *  *  * \n"
      "P5 *  *  *  *  *  *  *  *  * \n"
      "P6 *  *  *  *  *  *  *  *  * \n"
      "P7 *  *  *  *  * -FU *  *  * \n"
      "P8 *  *  *  *  *  *  *  *  * \n"
      "P9 *  *  *  * +OU *  *  *  * \n"
      "P+00KI\n"
      "P-00AL\n"
      "+\n"));
    King8Info black=state.king8Info(BLACK);
    King8Info white=state.king8Info(WHITE);
    // count
    TEST_CHECK_EQUAL(1, std::popcount(dropCandidate(black)));
    TEST_CHECK_EQUAL(1, std::popcount(dropCandidate(white)));
    // direction -- seems to be a view from king
    TEST_CHECK_EQUAL(one_hot(UR), dropCandidate(black));
    TEST_CHECK_EQUAL(one_hot(UL), dropCandidate(white));
  }
  {
    EffectState state(csa::read_board(
      "P1 *  *  *  * -OU *  *  *  * \n"
      "P2 *  *  *  *  *  *  *  *  * \n"
      "P3 *  *  * +FU *  *  *  *  * \n"
      "P4 *  *  *  *  *  *  *  *  * \n"
      "P5 *  *  *  *  *  *  *  *  * \n"
      "P6 *  *  *  *  *  *  *  *  * \n"
      "P7 *  *  * -FU *  *  *  *  * \n"
      "P8 *  *  *  *  *  *  *  *  * \n"
      "P9 *  *  *  * +OU *  *  *  * \n"
      "P+00KI\n"
      "P-00AL\n"
      "+\n"));
    King8Info black=state.king8Info(BLACK);
    King8Info white=state.king8Info(WHITE);
    // direction
    TEST_CHECK_EQUAL(one_hot(UL), dropCandidate(black));
    TEST_CHECK_EQUAL(one_hot(UR), dropCandidate(white));
  }
  {
    EffectState state(csa::read_board(
      "P1 *  *  *  *  *  *  *  *  * \n"
      "P2 *  *  *  *  *  *  *  *  * \n"
      "P3 *  *  * +FU-OU *  *  *  * \n"
      "P4 *  *  *  *  *  *  *  *  * \n"
      "P5 *  *  *  *  *  *  *  *  * \n"
      "P6 *  *  *  *  *  *  *  *  * \n"
      "P7 *  *  * -FU+OU *  *  *  * \n"
      "P8 *  *  *  *  *  *  *  *  * \n"
      "P9 *  *  *  *  *  *  *  *  * \n"
      "P+00KI\n"
      "P-00AL\n"
      "+\n"));
    King8Info black=state.king8Info(BLACK);
    King8Info white=state.king8Info(WHITE);
    // direction
    TEST_CHECK_EQUAL(one_hot(DL), dropCandidate(black));
    TEST_CHECK_EQUAL(one_hot(DR), dropCandidate(white));
  }
  {
    EffectState state=csa::read_board(
      "P1-KY-KE+GI+KA *  * +RY * -KY\n"
      "P2 *  * -OU * -KI * +NK *  * \n"
      "P3-FU * -GI-FU-FU-FU *  * -FU\n"
      "P4 *  * -FU *  *  *  *  *  * \n"
      "P5 *  *  *  * +KA *  *  *  * \n"
      "P6 *  *  *  *  *  *  *  *  * \n"
      "P7+FU * +FU+FU+FU+FU+FU * +FU\n"
      "P8 *  * -NK * +OU *  *  *  * \n"
      "P9+KY+KE * -HI * +KI+GI * +KY\n"
      "P+00FU00FU00FU\n"
      "P-00KI00KI00GI00FU00FU\n"
      "-\n"
      );
    // +0027KE or +5528UM
    King8Info king8=to_king8info(BLACK,state);
    TEST_CHECK_EQUAL(one_hot(D),liberty(king8));
    TEST_CHECK_EQUAL(1u,libertyCount(king8));
  }
  {
    EffectState state=csa::read_board(
      "P1-KY * -GI-KI * +HI * -KE-KY\n"
      "P2 *  *  *  * -OU * +NK *  * \n"
      "P3-FU * -FU-FU-FU-FU-FU * -FU\n"
      "P4 *  *  *  *  *  *  *  *  * \n"
      "P5 *  *  *  * -KA *  *  *  * \n"
      "P6 *  *  *  *  *  * +FU *  * \n"
      "P7+FU *  * +FU+FU+FU+GI * +FU\n"
      "P8 *  * -NK * +KI * +OU *  * \n"
      "P9+KY * -RY *  * -KA-GI+KE+KY\n"
      "P+00KI00KI00GI00FU00FU\n"
      "P-00FU00FU00FU\n"
      "+\n"
      );
    // +0027KE or +5528UM
    King8Info king8=to_king8info(WHITE,state);
    TEST_CHECK_EQUAL(one_hot(D),liberty(king8));
    TEST_CHECK_EQUAL(1u,libertyCount(king8));
  }
  {
    EffectState state=csa::read_board(
      "P1 *  *  * -KI *  *  *  * -KY\n"
      "P2 * +KI-GI *  *  *  *  *  * \n"
      "P3+GI-FU-FU *  *  *  *  * -FU\n"
      "P4-FU * -KE *  * -FU-FU-FU * \n"
      "P5 *  * +KA *  *  *  *  *  * \n"
      "P6 *  *  *  * -FU *  * +HI * \n"
      "P7 * +FU+KE+FU * +KA-OU * +FU\n"
      "P8 *  * +OU+GI *  *  *  *  * \n"
      "P9 *  * +KI * -NG+KY *  * +KY\n"
      "P+00KE00FU\n"
      "P-00HI00KI00KE00KY00FU00FU00FU00FU00FU00FU\n"
      "+\n"
      );
    // +0027KE or +5528UM
    King8Info king8=to_king8info(BLACK,state);
    TEST_CHECK_EQUAL((one_hot(DL)),liberty(king8));
    TEST_CHECK_EQUAL((one_hot(DR))|one_hot(D)|one_hot(L)|(one_hot(UL))|one_hot(U)|(one_hot(UR)),
                     dropCandidate(king8));
    TEST_CHECK_EQUAL(255u,libertyCandidate(king8));
    TEST_CHECK_EQUAL((one_hot(DR))|one_hot(D)|one_hot(L)|(one_hot(UL))|one_hot(U)|(one_hot(UR)),
                     moveCandidate2(king8));
    TEST_CHECK_EQUAL((one_hot(DR))|one_hot(D)|one_hot(L)|(one_hot(UL))|one_hot(U)|(one_hot(UR)),
                     spaces(king8));
    TEST_CHECK_EQUAL((one_hot(DR))|one_hot(D)|one_hot(L)|(one_hot(UL))|one_hot(U)|(one_hot(UR)),
                     moves(king8));
    TEST_CHECK_EQUAL(1u,libertyCount(king8));
  }
  {
    EffectState state=csa::read_board(
      "P1+NY+TO *  *  *  * -OU-KE-KY\n"
      "P2 *  *  *  *  * -GI-KI *  *\n"
      "P3 * +RY *  * +UM * -KI-FU-FU\n"
      "P4 *  * +FU-FU *  *  *  *  *\n"
      "P5 *  * -KE * +FU *  * +FU *\n"
      "P6+KE *  * +FU+GI-FU *  * +FU\n"
      "P7 *  * -UM *  *  *  *  *  *\n"
      "P8 *  *  *  *  *  *  *  *  * \n"
      "P9 * +OU * -GI *  *  *  * -NG\n"
      "P+00HI00KI00KE00KY00FU00FU00FU00FU00FU00FU\n"
      "P-00KI00KY00FU00FU\n"
      "P-00AL\n"
      "+\n"
      );
    King8Info king8=to_king8info(BLACK,state);
    TEST_CHECK_EQUAL((one_hot(UL))|one_hot(R),
			 liberty(king8));
    TEST_CHECK_EQUAL(0x00u,dropCandidate(king8));
    TEST_CHECK_EQUAL((one_hot(UL))|one_hot(R),
			 libertyCandidate(king8));
    TEST_CHECK_EQUAL(0x00u,moveCandidate2(king8));
    TEST_CHECK_EQUAL((one_hot(UL))|one_hot(R),spaces(king8));
    TEST_CHECK_EQUAL((one_hot(UR)),moves(king8));
    TEST_CHECK_EQUAL(one_hot(U),libertyCount(king8));
  }
  {
    EffectState state=csa::read_board(
      "P1 *  *  * -KI *  *  *  * -KY\n"
      "P2 * +KI-GI *  *  *  *  *  * \n"
      "P3+GI-FU-FU *  *  *  *  * -FU\n"
      "P4-FU * -KE *  * -FU-FU-FU * \n"
      "P5 *  *  *  *  *  *  *  *  * \n"
      "P6 *  *  *  * -FU *  * +HI * \n"
      "P7 * +FU+KE+FU * +KA-OU * +FU\n"
      "P8 *  * +OU+GI * +KA *  *  * \n"
      "P9 *  * +KI * -NG+KY *  * +KY\n"
      "P+00KE00FU\n"
      "P-00HI00KI00KE00KY00FU00FU00FU00FU00FU00FU\n"
      "-\n"
      );
    // +0027KE or +5528UM
    King8Info king8=to_king8info(BLACK,state);
    TEST_CHECK_EQUAL(one_hot(R),liberty(king8));
    TEST_CHECK_EQUAL((one_hot(DR))|one_hot(D)|one_hot(L)|(one_hot(UL))|one_hot(U),
			 dropCandidate(king8));
    TEST_CHECK_EQUAL(255u,libertyCandidate(king8));
    TEST_CHECK_EQUAL((one_hot(DR))|one_hot(D)|one_hot(L)|(one_hot(UL))|one_hot(U),
			 moveCandidate2(king8));
    TEST_CHECK_EQUAL((one_hot(DR))|one_hot(D)|one_hot(L)|(one_hot(UL))|one_hot(U),
			 spaces(king8));
    TEST_CHECK_EQUAL((one_hot(DR))|one_hot(D)|one_hot(L)|(one_hot(UL))|one_hot(U),
			 moves(king8));
    TEST_CHECK_EQUAL((one_hot(UL)),libertyCount(king8));
  }
}

void test_state() {
  {
    SimpleState state;
    TEST_ASSERT(state.isConsistent());
  }
  {
    EffectState state;
    TEST_ASSERT(state.isConsistent());
  }
}

void testEffectedState(EffectState const& state,Move move)
{
  PieceMask b_mask=state.effectedPieces(BLACK);
  PieceMask w_mask=state.effectedPieces(WHITE);
  for (Piece p: state.all_pieces()) {
    if(p.isOnBoard()){
      Square pos=p.square();
      if(b_mask.test(p.id())){
        if(!state.hasEffectAt(BLACK,pos)){
          std::cerr << std::endl << state << std::endl;
          std::cerr << "b_mask=" << b_mask << ",num=" << p.id() << ",pos=" << pos << ",move=" << move << std::endl;
        }
        TEST_CHECK(state.hasEffectAt(BLACK,pos));
      }
      else{
        if(state.hasEffectAt(BLACK,pos)){
          std::cerr << std::endl << state << std::endl;
          std::cerr << "b_mask=" << b_mask << ",num=" << p.id() << ",pos=" << pos << ",move=" << move << std::endl;
        }
        TEST_CHECK(!state.hasEffectAt(BLACK,pos));
      }
      if(w_mask.test(p.id())){
        if(!state.hasEffectAt(WHITE,pos)){
          std::cerr << std::endl << state << std::endl;
          std::cerr << "w_mask=" << w_mask << ",num=" << p.id() << ",pos=" << pos << ",move=" << move << std::endl;
        }
        TEST_CHECK(state.hasEffectAt(WHITE,pos));
      }
      else{
        if(state.hasEffectAt(WHITE,pos)){
          std::cerr << std::endl << state << std::endl;
          std::cerr << "w_mask=" << w_mask << ",num=" << p.id() << ",pos=" << pos << ",move=" << move << std::endl;
        }
        TEST_CHECK(!state.hasEffectAt(WHITE,pos));
      }
    }
  }
}
void test_effect_state() {
  {
    EffectState state;
    TEST_CHECK(state.pieceAt(Square::STAND()).isEdge());
    TEST_CHECK(state.pieceAt(Square::makeDirect(0)).isEdge());
  }
  {
    EffectState state(csa::read_board(
                                   "P1-KY * +UM *  *  *  * -KE-KY\n"
                                   "P2 *  *  *  *  *  *  * -OU * \n"
                                   "P3 *  *  *  *  * -HI * -FU-FU\n"
                                   "P4-FU * -FU * -FU-KI-FU-GI * \n"
                                   "P5 *  *  *  *  *  *  *  * +FU\n"
                                   "P6+FU+FU+FU+KI+FU * +FU *  * \n"
                                   "P7 * +KI * +FU *  * -UM *  * \n"
                                   "P8 * +OU * +GI *  * -NG *  * \n"
                                   "P9+KY+KE *  *  *  *  *  * -RY\n"
                                   "P+00KI00GI00KY00FU\n"
                                   "P-00KE00KE00FU00FU00FU00FU\n"
                                   "+\n"));
    TEST_CHECK(state.isConsistent());
    TEST_CHECK(state.hasEffectIf(newPtypeO(BLACK,PBISHOP),
                                 Square(8,2),
                                 Square(3,7)));
  }
  {
    EffectState state(csa::read_board(
                                   "P1+NY+TO *  *  *  * -OU-KE-KY\n"
                                   "P2 *  *  *  *  * -GI-KI *  *\n"
                                   "P3 * +RY *  * +UM * -KI-FU-FU\n"
                                   "P4 *  * +FU-FU *  *  *  *  *\n"
                                   "P5 *  * -KE * +FU *  * +FU *\n"
                                   "P6-KE *  * +FU+GI-FU *  * +FU\n"
                                   "P7 *  * -UM *  *  *  *  *  *\n"
                                   "P8 *  *  *  *  *  *  *  *  * \n"
                                   "P9 * +OU * -GI *  *  *  * -NG\n"
                                   "P+00HI00KI00KE00KY00FU00FU00FU00FU00FU00FU\n"
                                   "P-00KI00KY00FU00FU\n"
                                   "P-00AL\n"
                                   "+\n"));
    TEST_CHECK(state.isConsistent());
    // (8,3)の竜
    TEST_CHECK(state.hasEffectIf(newPtypeO(BLACK,PROOK),Square(8,3),Square(8,9)));
    TEST_CHECK(!state.hasEffectIf(newPtypeO(BLACK,PROOK),Square(8,3),Square(3,3)));
    // (4,2)の銀
    TEST_CHECK(state.hasEffectIf(newPtypeO(WHITE,SILVER),Square(4,2),Square(5,3)));
    TEST_CHECK(!state.hasEffectIf(newPtypeO(WHITE,SILVER),Square(4,2),Square(3,2)));
    // (8,2)の竜, 空白でも可能
    TEST_CHECK(state.hasEffectIf(newPtypeO(BLACK,PROOK),Square(8,2),Square(7,1)));
  }
  const EffectState test_position(csa::read_board(
                                               "P1-KY-KE *  *  *  *  *  *  * \n"
                                               "P2 *  *  *  *  *  * -KI-OU * \n"
                                               "P3 *  * -FU *  * -KI *  *  * \n"
                                               "P4-FU-HI *  *  *  *  * +GI-KY\n"
                                               "P5 * -FU+FU * -FU-FU-FU-FU+FU\n"
                                               "P6+FU *  *  *  *  * +KE *  * \n"
                                               "P7 * +FU * +FU * +FU+FU+FU-KY\n"
                                               "P8+KY *  *  *  * +KI+KI *  * \n"
                                               "P9 * +KE-UM * +KA *  * +KE+OU\n"
                                               "P-00FU\n"
                                               "P+00FU\n"
                                               "P-00FU\n"
                                               "P-00GI\n"
                                               "P-00GI\n"
                                               "P-00GI\n"
                                               "P-00HI\n"
                                               "+\n"
                                               ));
  TEST_CHECK(test_position.isConsistent());
  {
    EffectState state0 = test_position;
    TEST_CHECK(state0.isConsistent());
    {
      // (7,9)の馬
      Piece p=state0.pieceAt(Square(7,9));
      TEST_CHECK(state0.hasEffectByPiece(p,Square(3,5)));
      TEST_CHECK(state0.hasEffectByPiece(p,Square(8,9)));
      TEST_CHECK(state0.hasEffectByPiece(p,Square(9,7)));
      TEST_CHECK(!state0.hasEffectByPiece(p,Square(7,9)));
      TEST_CHECK(!state0.hasEffectByPiece(p,Square(2,4)));
      TEST_CHECK(!state0.hasEffectByPiece(p,Square(9,9)));
    }
    {
      Piece p=state0.pieceAt(Square(2,2));
      TEST_CHECK(state0.hasEffectByPiece(p,Square(1,3)));
      TEST_CHECK(state0.hasEffectByPiece(p,Square(1,1)));
      TEST_CHECK(!state0.hasEffectByPiece(p,Square(2,2)));
      TEST_CHECK(!state0.hasEffectByPiece(p,Square(4,4)));
    }
  }
  {
    EffectState state(csa::read_board(
                                   "P1+NY+TO *  *  *  * -OU-KE-KY\n"
                                   "P2 *  *  *  *  * -GI-KI *  *\n"
                                   "P3 * +RY *  * +UM * -KI-FU-FU\n"
                                   "P4 *  * +FU-FU *  *  *  *  *\n"
                                   "P5 *  * -KE * +FU *  * +FU *\n"
                                   "P6-KE *  * +FU+GI-FU *  * +FU\n"
                                   "P7 *  * -UM *  *  *  *  *  *\n"
                                   "P8 *  *  *  *  *  *  *  *  * \n"
                                   "P9 * +OU * -GI *  *  *  * -NG\n"
                                   "P+00HI00KI00KE00KY00FU00FU00FU00FU00FU00FU\n"
                                   "P-00KI00KY00FU00FU\n"
                                   "P-00AL\n"
                                   "+\n"));
    
    {
      // (7,7)の馬
      Piece p=state.pieceAt(Square(7,7));
      TEST_CHECK(state.hasEffectByPiece(p,Square(9,5)));
      TEST_CHECK(state.hasEffectByPiece(p,Square(7,6)));
      TEST_CHECK(state.hasEffectByPiece(p,Square(6,6)));
      TEST_CHECK(!state.hasEffectByPiece(p,Square(5,5)));
      TEST_CHECK(!state.hasEffectByPiece(p,Square(7,5)));
      TEST_CHECK(!state.hasEffectByPiece(p,Square(7,7)));
    }
    {
      Piece p=state.pieceAt(Square(1,1));
      TEST_CHECK(state.hasEffectByPiece(p,Square(1,3)));
      TEST_CHECK(!state.hasEffectByPiece(p,Square(1,1)));
      TEST_CHECK(!state.hasEffectByPiece(p,Square(2,3)));
      TEST_CHECK(!state.hasEffectByPiece(p,Square(1,4)));
    }
  }
  {
    EffectState state0 = test_position;
    // 7-3 には後手の利きがある．
    TEST_CHECK((state0.hasEffectAt(WHITE,Square(7,3))));
    // 7-1 には後手の利きがない
    TEST_CHECK(!(state0.hasEffectAt(WHITE,Square(7,1))));
    // 9-5 には先手の利きがある．
    TEST_CHECK((state0.hasEffectAt(BLACK,Square(9,5))));
    // 9-8 には後手の利きがない
    TEST_CHECK(!(state0.hasEffectAt(BLACK,Square(9,8))));
  }
  {
    EffectState state0 = test_position;
    // 9-3 は後手の香車(9-1),桂馬(8-1)の利きがある．
    TEST_CHECK((state0.hasEffectNotBy(WHITE,state0.pieceAt(Square(9,1)),Square(9,3))));
    // 9-2 は後手の香車(9-1)利きのみ
    TEST_CHECK(!(state0.hasEffectNotBy(WHITE,state0.pieceAt(Square(9,1)),Square(9,2))));
  }
  {
    EffectState state0 = test_position;
    {
      Piece p=state0.findCheapAttack(WHITE,Square(5,7));
      TEST_CHECK(p==state0.pieceAt(Square(7,9)));
    }
    {
      // 2,3への利きはどちらでも良し
      Piece p=state0.findCheapAttack(WHITE,Square(2,3));
      TEST_CHECK(p==state0.pieceAt(Square(2,2)) ||
                 p==state0.pieceAt(Square(3,2)));
    }
    {
      //利きがない場合は呼んで良い
      Piece p=state0.findCheapAttack(WHITE,Square(5,9));
      TEST_CHECK(p==Piece::EMPTY());
    }
  }
  {
    {
      EffectState state(csa::read_board(
                                     "P1-KY-KE * -KI * -OU * -KE-KY\n"
                                     "P2 *  *  *  *  *  * -KI *  * \n"
                                     "P3-FU *  * -FU-FU-FU * -FU-FU\n"
                                     "P4 *  * -FU * -GI * -FU-GI * \n"
                                     "P5 * -FU * +FU *  *  *  *  * \n"
                                     "P6-KA+FU+FU-HI *  *  *  *  * \n"
                                     "P7+FU+KI+KE *  * +FU+FU * +FU\n"
                                     "P8 *  *  * +GI *  * +GI+HI+KA\n"
                                     "P9+KY *  * +OU * +KI * +KE+KY\n"
                                     "P+00FU\n"
                                     "P-00FU\n"
                                     "+\n"));
      // 68銀はpinned
      TEST_CHECK(!state.hasEffectByNotPinned(BLACK,Square(5,7)));
      // 67は銀で取れるが，pinnedな銀なのでfalse
      TEST_CHECK(!state.hasEffectByNotPinned(BLACK,Square(6,7)));
      // 98は関係ない香車による利きあり
      TEST_CHECK(state.hasEffectByNotPinned(BLACK,Square(9,8)));
      // 88はpinnedの金による利きのみ
      TEST_CHECK(!state.hasEffectByNotPinned(BLACK,Square(8,8)));
    
    }
  }
  {
    EffectState state=csa::read_board(
                                   "P1-KY-KE * -KI-OU-KI-GI-KE-KY\n"
                                   "P2 * -HI *  *  *  *  * -KA * \n"
                                   "P3-FU-FU * -FU-FU-FU-FU-FU-FU\n"
                                   "P4 *  * -FU-GI *  *  *  *  * \n"
                                   "P5 *  *  *  *  *  *  *  *  * \n"
                                   "P6 *  * +FU *  *  *  *  *  * \n"
                                   "P7+FU+FU * +FU+FU+FU+FU+FU+FU\n"
                                   "P8 * +KA+KI *  *  *  * +HI * \n"
                                   "P9+KY+KE+GI * +OU+KI+GI+KE+KY\n"
                                   "+\n");
    TEST_CHECK_EQUAL(2, std::min(2, state.countEffect(BLACK, Square(7,7))));
    TEST_CHECK_EQUAL(2, std::min(2, state.countEffect(BLACK, Square(6,6))));
    TEST_CHECK_EQUAL(1, std::min(2, state.countEffect(BLACK, Square(5,6))));
    TEST_CHECK_EQUAL(0, std::min(2, state.countEffect(BLACK, Square(6,5))));
    TEST_CHECK_EQUAL(2, std::min(2, state.countEffect(WHITE, Square(7,3))));
    TEST_CHECK_EQUAL(2, std::min(2, state.countEffect(WHITE, Square(7,5))));
    TEST_CHECK_EQUAL(1, std::min(2, state.countEffect(WHITE, Square(6,5))));
    TEST_CHECK_EQUAL(1, std::min(2, state.countEffect(WHITE, Square(5,5))));
    TEST_CHECK_EQUAL(0, std::min(2, state.countEffect(WHITE, Square(4,5))));
  }
  {
    EffectState state=csa::read_board(
                                   "P1-KY-KE * -KI-OU-KI-GI-KE-KY\n"
                                   "P2 * -HI *  *  *  *  * -KA * \n"
                                   "P3-FU-FU * -FU-FU-FU-FU-FU-FU\n"
                                   "P4 *  * -FU-GI *  *  *  *  * \n"
                                   "P5 *  *  *  *  *  *  *  *  * \n"
                                   "P6 *  * +FU *  *  *  *  *  * \n"
                                   "P7+FU+FU * +FU+FU+FU+FU+FU+FU\n"
                                   "P8 * +KA+KI *  *  *  * +HI * \n"
                                   "P9+KY+KE+GI * +OU+KI+GI+KE+KY\n"
                                   "+\n");
    TEST_CHECK_EQUAL(3, state.countEffect(BLACK, Square(7,7)));
    TEST_CHECK_EQUAL(2, state.countEffect(BLACK, Square(6,6)));
    TEST_CHECK_EQUAL(1, state.countEffect(BLACK, Square(5,6)));
    TEST_CHECK_EQUAL(0, state.countEffect(BLACK, Square(6,5)));
  
    TEST_CHECK_EQUAL(2, state.countEffect(WHITE, Square(7,3)));
    TEST_CHECK_EQUAL(2, state.countEffect(WHITE, Square(7,5)));
    TEST_CHECK_EQUAL(1, state.countEffect(WHITE, Square(6,5)));
    TEST_CHECK_EQUAL(1, state.countEffect(WHITE, Square(5,5)));
    TEST_CHECK_EQUAL(0, state.countEffect(WHITE, Square(4,5)));
    TEST_CHECK_EQUAL(4, state.countEffect(WHITE, Square(5,2)));
  }
  {
    EffectState state=csa::read_board(
                                   "P1-KY * -KI * -KE *  *  * +RY\n"
                                   "P2 * -OU * -GI+GI *  *  *  * \n"
                                   "P3 * -GI * -KI *  *  *  *  * \n"
                                   "P4-FU+KE-FU-FU-FU * +KA * -FU\n"
                                   "P5 * -FU *  *  * +FU *  *  * \n"
                                   "P6+FU * +FU+FU+FU *  *  * +FU\n"
                                   "P7 * +FU *  *  * +GI *  *  * \n"
                                   "P8 * +OU+KI-RY *  * +FU *  * \n"
                                   "P9+KY *  *  *  *  *  *  * +KY\n"
                                   "P+00KA00KE00KY00FU00FU00FU\n"
                                   "P-00KI00KE00FU\n"
                                   "-\n");
    TEST_CHECK_EQUAL(2, state.countEffect(BLACK, Square(7,7)));
    PieceMask pin;
    pin.set(state.pieceOnBoard(Square(7,8)).id());
    TEST_CHECK_EQUAL(1, state.countEffect(BLACK, Square(7,7), pin));
  }
  {
    EffectState state=csa::read_board(
                                   "P1+NY+TO *  *  *  * -OU-KE-KY\n"
                                   "P2 *  *  *  *  * -GI-KI *  *\n"
                                   "P3 * +RY *  * +UM * -KI-FU-FU\n"
                                   "P4 *  * +FU-FU *  *  *  *  *\n"
                                   "P5 *  * -KE * +FU *  * +FU *\n"
                                   "P6-KE *  * +FU+GI-FU *  * +FU\n"
                                   "P7 *  * -UM *  *  *  *  *  *\n"
                                   "P8 *  *  *  *  *  *  *  *  * \n"
                                   "P9 * +OU * -GI *  *  *  * -NG\n"
                                   "P+00HI00KI00KE00KY00FU00FU00FU00FU00FU00FU\n"
                                   "P-00KI00KY00FU00FU\n"
                                   "P-00AL\n"
                                   "+\n");
    TEST_CHECK(! state.inCheck(BLACK));
    TEST_CHECK(! state.inCheck(WHITE));
    state.makeMove(Move(Square(8,1),Square(7,1),PPAWN,Ptype_EMPTY,false,BLACK));
    TEST_CHECK(! state.inCheck(BLACK));
    TEST_CHECK(! state.inCheck(WHITE));
    state.makeMove(Move(Square::STAND(),Square(8,8),PAWN,Ptype_EMPTY,false,WHITE));
    TEST_CHECK(state.inCheck(BLACK));
    TEST_CHECK(! state.inCheck(WHITE));
    state.makeMove(Move(Square(7,1),Square(8,1),PPAWN,Ptype_EMPTY,false,BLACK));
    TEST_CHECK(state.inCheck(BLACK));
    TEST_CHECK(! state.inCheck(WHITE));
  }

  {
    EffectState state(csa::read_board(
                                   "P1-KY-KE-GI-KI-OU-KI-GI-KE-KY\n"
                                   "P2 * -HI *  *  *  *  * -KA * \n"
                                   "P3-FU-FU-FU-FU-FU-FU * -FU-FU\n"
                                   "P4 *  *  *  *  *  * -FU *  * \n"
                                   "P5 *  *  *  *  *  *  *  *  * \n"
                                   "P6 *  * +FU *  *  *  *  *  * \n"
                                   "P7+FU+FU * +FU+FU+FU+FU+FU+FU\n"
                                   "P8 * +KA *  *  *  *  * +HI * \n"
                                   "P9+KY+KE+GI+KI+OU+KI+GI+KE+KY\n"
                                   "+\n"));
    TEST_CHECK(state.isConsistent());
    TEST_CHECK(state.longEffectAt<LANCE>(Square(9,2)) != 0);
    TEST_MSG("Lance %s", std::bitset<64>(state.longEffectAt<LANCE>(Square(9,2))).to_string().c_str());
    TEST_CHECK(state.longEffectAt<ROOK>(Square(9,2)) != 0);
    TEST_CHECK(std::popcount(state.longEffectAt(Square(9,2), WHITE)) == 2);
  }

  {
    EffectState state=csa::read_board(
                                   "P1+NY+TO *  *  *  * -OU-KE-KY\n"
                                   "P2 *  *  *  *  * -GI-KI *  *\n"
                                   "P3 * +RY *  * +UM * -KI-FU-FU\n"
                                   "P4 *  * +FU-FU *  *  *  *  *\n"
                                   "P5 *  * -KE * +FU *  * +FU *\n"
                                   "P6-KE *  * +FU+GI-FU *  * +FU\n"
                                   "P7 *  * -UM *  *  *  *  *  *\n"
                                   "P8 *  *  *  *  *  *  *  *  * \n"
                                   "P9 * +OU * -GI *  *  *  * -NG\n"
                                   "P+00HI00KI00KE00KY00FU00FU00FU00FU00FU00FU\n"
                                   "P-00KI00KY00FU00FU\n"
                                   "P-00AL\n"
                                   "+\n");
    TEST_CHECK(state.isConsistent());
    testEffectedState(state,Move::INVALID());
    { // simple move
      EffectState state1=state;
      TEST_CHECK(state1.isConsistent());
      Move move(Square(8,3),Square(8,2),PROOK,Ptype_EMPTY,false,BLACK);
      state1.makeMove(move);
      TEST_ASSERT(state1.isConsistent());
      testEffectedState(state1,move);
    }
    { // drop move
      EffectState state1=state;
      Move move(Square(8,2),ROOK,BLACK);
      state1.makeMove(move);
      testEffectedState(state1,move);
    }
    { // capture move
      EffectState state1=state;
      Move move(Square(5,3),Square(4,2),PBISHOP,SILVER,false,BLACK);
      state1.makeMove(move);
      testEffectedState(state1,move);
    }
#if 0
    std::ifstream ifs(OslConfig::testCsaFile("FILES"));
    TEST_CHECK(ifs);
    int i=0;
    int count=100;
    if (OslConfig::inUnitTestShort())
      count=10;
    std::string filename;
    while ((ifs >> filename) && (++i<count)) {
      if (filename == "") 
        break;
      MiniRecord record=CsaFileMinimal(OslConfig::testCsaFile(filename)).load();
      EffectState state(record.initial_state);
      for(auto move:record.moves){
        state.makeMove(move);
        testEffectedState(state,move);
      }
    }
#endif
  }

  {
    EffectState state=csa::read_board(
                                   "P1+NY+TO *  *  *  * -OU-KE-KY\n"
                                   "P2 *  *  *  *  * -GI-KI *  *\n"
                                   "P3 * +RY *  * +UM * -KI-FU-FU\n"
                                   "P4 *  * +FU-FU *  *  *  *  *\n"
                                   "P5 *  * -KE * +FU *  * +FU *\n"
                                   "P6-KE *  * +FU+GI-FU *  * +FU\n"
                                   "P7 *  * -UM *  *  *  *  *  *\n"
                                   "P8 *  *  *  *  *  *  *  *  * \n"
                                   "P9 * +OU * -GI *  *  *  * -NG\n"
                                   "P+00HI00KI00KE00KY00FU00FU00FU00FU00FU00FU\n"
                                   "P-00KI00KY00FU00FU\n"
                                   "P-00AL\n"
                                   "+\n");
    TEST_CHECK_EQUAL(Square(8, 10), state.kingVisibilityBlackView(BLACK, U));
    TEST_CHECK_EQUAL(Square(8, 3), state.kingVisibilityBlackView(BLACK, D));
    TEST_CHECK_EQUAL(Square(6, 9), state.kingVisibilityBlackView(BLACK, L));
    TEST_CHECK_EQUAL(Square(10, 9), state.kingVisibilityBlackView(BLACK, R));
    TEST_CHECK_EQUAL(Square(5, 6), state.kingVisibilityBlackView(BLACK, DL));
    TEST_CHECK_EQUAL(Square(7, 10), state.kingVisibilityBlackView(BLACK, UL));
    TEST_CHECK_EQUAL(Square(10, 7), state.kingVisibilityBlackView(BLACK, DR));
    TEST_CHECK_EQUAL(Square(9, 10), state.kingVisibilityBlackView(BLACK, UR));

    TEST_CHECK_EQUAL(Square(3, 2), state.kingVisibilityBlackView(WHITE, U));
    TEST_CHECK_EQUAL(Square(3, 0), state.kingVisibilityBlackView(WHITE, D));
    TEST_CHECK_EQUAL(Square(2, 1), state.kingVisibilityBlackView(WHITE, L));
    TEST_CHECK_EQUAL(Square(8, 1), state.kingVisibilityBlackView(WHITE, R));
    TEST_CHECK_EQUAL(Square(4, 2), state.kingVisibilityBlackView(WHITE, UR));
    TEST_CHECK_EQUAL(Square(4, 0), state.kingVisibilityBlackView(WHITE, DR));
    TEST_CHECK_EQUAL(Square(1, 3), state.kingVisibilityBlackView(WHITE, UL));
    TEST_CHECK_EQUAL(Square(2, 0), state.kingVisibilityBlackView(WHITE, DL));

    TEST_CHECK_EQUAL(Square(8, 3), state.kingVisibilityOfPlayer(BLACK, U));
    TEST_CHECK_EQUAL(Square(8, 10), state.kingVisibilityOfPlayer(BLACK, D));
    TEST_CHECK_EQUAL(Square(10, 9), state.kingVisibilityOfPlayer(BLACK, L));
    TEST_CHECK_EQUAL(Square(6, 9), state.kingVisibilityOfPlayer(BLACK, R));
    TEST_CHECK_EQUAL(Square(5, 6), state.kingVisibilityOfPlayer(BLACK, UR));
    TEST_CHECK_EQUAL(Square(9, 10), state.kingVisibilityOfPlayer(BLACK, DL));
    TEST_CHECK_EQUAL(Square(10, 7), state.kingVisibilityOfPlayer(BLACK, UL));
    TEST_CHECK_EQUAL(Square(7, 10), state.kingVisibilityOfPlayer(BLACK, DR));

    TEST_CHECK_EQUAL(Square(3, 2), state.kingVisibilityOfPlayer(WHITE, U));
    TEST_CHECK_EQUAL(Square(3, 0), state.kingVisibilityOfPlayer(WHITE, D));
    TEST_CHECK_EQUAL(Square(2, 1), state.kingVisibilityOfPlayer(WHITE, L));
    TEST_CHECK_EQUAL(Square(8, 1), state.kingVisibilityOfPlayer(WHITE, R));
    TEST_CHECK_EQUAL(Square(4, 2), state.kingVisibilityOfPlayer(WHITE, UR));
    TEST_CHECK_EQUAL(Square(4, 0), state.kingVisibilityOfPlayer(WHITE, DR));
    TEST_CHECK_EQUAL(Square(1, 3), state.kingVisibilityOfPlayer(WHITE, UL));
    TEST_CHECK_EQUAL(Square(2, 0), state.kingVisibilityOfPlayer(WHITE, DL));
  }
  {
    {
      EffectState state(csa::read_board(
                                     "P1-KY-KE *  *  *  *  *  * -KY\n"
                                     "P2 * -HI *  *  * -KI-OU *  * \n"
                                     "P3-FU *  * +TO+FU-KI *  *  * \n"
                                     "P4 *  * -FU * -FU-GI * -FU-FU\n"
                                     "P5 * -FU *  * -GI-KE-FU *  * \n"
                                     "P6 *  * +FU *  * -FU *  * +FU\n"
                                     "P7+FU+FU+KE *  *  * +FU+FU * \n"
                                     "P8 *  * +KI * +GI * +KI+OU * \n"
                                     "P9+KY *  *  * +HI *  * +KE+KY\n"
                                     "P+00KA00KA00FU\n"
                                     "P-00GI00FU\n"
                                     "+\n"));
      TEST_CHECK_EQUAL(state.pieceAt(Square(5,3)), 
                       state.findCheapAttack(BLACK, Square(5,2)));
      TEST_CHECK_EQUAL(state.pieceAt(Square(6,3)), 
                       state.findCheapAttack(BLACK, Square(6,2)));
      TEST_CHECK_EQUAL(Piece::EMPTY(),
                       state.findCheapAttack(BLACK, Square(5,1)));
      TEST_CHECK_EQUAL(state.pieceAt(Square(2,9)), 
                       state.findCheapAttack(BLACK, Square(3,7)));
      TEST_CHECK_EQUAL(state.pieceAt(Square(5,8)), 
                       state.findCheapAttack(BLACK, Square(4,9)));
      TEST_CHECK_EQUAL(state.pieceAt(Square(5,8)), 
                       state.findCheapAttack(BLACK, Square(4,7)));
      TEST_CHECK_EQUAL(state.pieceAt(Square(1,9)), 
                       state.findCheapAttack(BLACK, Square(1,7)));
      TEST_CHECK_EQUAL(state.pieceAt(Square(5,9)), 
                       state.findCheapAttack(BLACK, Square(9,9)));
      TEST_CHECK_EQUAL(state.pieceAt(Square(5,9)), 
                       state.findCheapAttack(BLACK, Square(2,9)));
      TEST_CHECK_EQUAL(state.pieceAt(Square(2,8)), 
                       state.findCheapAttack(BLACK, Square(1,9)));

      TEST_CHECK_EQUAL(state.pieceAt(Square(5,4)), 
                       state.findCheapAttack(WHITE, Square(5,5)));
    }
  }
  {
    {
      EffectState state;
      TEST_CHECK_EQUAL(Piece::EMPTY(), state.findThreatenedPiece(BLACK));
      TEST_CHECK_EQUAL(Piece::EMPTY(), state.findThreatenedPiece(WHITE));
    }
    {
      EffectState state(csa::read_board(
                                     "P1-KY-KE-GI-KI-OU-KI-GI-KE-KY\n"
                                     "P2 * -HI *  *  *  *  * -KA * \n"
                                     "P3-FU-FU-FU-FU-FU-FU-FU-FU * \n"
                                     "P4 *  *  *  *  *  *  *  *  * \n"
                                     "P5 *  *  *  *  *  *  *  *  * \n"
                                     "P6 *  *  *  *  *  *  *  *  * \n"
                                     "P7 * +FU+FU+FU+FU+FU+FU+FU+FU\n"
                                     "P8 * +KA *  *  *  *  * +HI * \n"
                                     "P9+KY+KE+GI+KI+OU+KI+GI+KE+KY\n"
                                     "P+00FU00FU\n"
                                     "+\n")); 
      TEST_ASSERT(state.pieceAt(Square(1,7)).isPiece());
      // std::cerr << state.findThreatenedPiece(BLACK) << "\n";
      TEST_ASSERT(state.pieceAt(Square(1,7)) == state.findThreatenedPiece(BLACK));
      TEST_CHECK_EQUAL(state.pieceAt(Square(9,3)), state.findThreatenedPiece(WHITE));
    }
    {
      EffectState state(csa::read_board(
                                     "P1-KY-KE-GI-KI-OU-KI-GI * -KY\n"
                                     "P2 * -HI *  *  *  *  * -KA * \n"
                                     "P3 * -FU-FU-FU-FU-FU-FU-FU * \n"
                                     "P4 *  *  *  * +KE *  *  *  * \n"
                                     "P5 *  *  *  *  *  *  *  *  * \n"
                                     "P6 *  *  *  * -KE *  *  *  * \n"
                                     "P7 * +FU+FU+FU+FU+FU+FU+FU * \n"
                                     "P8 * +KA *  *  *  *  * +HI * \n"
                                     "P9+KY * +GI+KI+OU+KI+GI+KE+KY\n"
                                     "P+00FU00FU00FU00FU\n"
                                     "+\n")); 
      TEST_CHECK_EQUAL(state.pieceAt(Square(5,4)), state.findThreatenedPiece(BLACK));
      TEST_CHECK_EQUAL(state.pieceAt(Square(5,6)), state.findThreatenedPiece(WHITE));
    }
    {
      EffectState state(csa::read_board(
                                     "P1-KY-KE-GI-KI-OU-KI *  * -KY\n"
                                     "P2 * -HI *  *  *  *  * -KA * \n"
                                     "P3 * -FU-FU-FU-FU-FU-FU-FU * \n"
                                     "P4 *  *  * +GI+KE *  *  *  * \n"
                                     "P5 *  *  *  *  *  *  *  *  * \n"
                                     "P6 *  *  *  * -KE-GI *  *  * \n"
                                     "P7 * +FU+FU+FU+FU+FU+FU+FU * \n"
                                     "P8 * +KA *  *  *  *  * +HI * \n"
                                     "P9+KY *  * +KI+OU+KI+GI+KE+KY\n"
                                     "P+00FU00FU00FU00FU\n"
                                     "+\n")); 
      TEST_CHECK_EQUAL(state.pieceAt(Square(6,4)), state.findThreatenedPiece(BLACK));
      TEST_CHECK_EQUAL(state.pieceAt(Square(4,6)), state.findThreatenedPiece(WHITE));
    }
    {
      EffectState state(csa::read_board(
                                     "P1-KY-KE-GI * -OU-KI *  * -KY\n"
                                     "P2 * -HI *  *  *  *  * -KA * \n"
                                     "P3 * -FU-FU-FU-FU-FU-FU-FU * \n"
                                     "P4 *  * +KI+GI+KE *  *  *  * \n"
                                     "P5 *  *  *  *  *  *  *  *  * \n"
                                     "P6 *  *  *  * -KE-GI-KI *  * \n"
                                     "P7 * +FU+FU+FU+FU+FU+FU+FU * \n"
                                     "P8 * +KA *  *  *  *  * +HI * \n"
                                     "P9+KY *  * +KI+OU * +GI+KE+KY\n"
                                     "P+00FU00FU00FU00FU\n"
                                     "+\n")); 
      TEST_CHECK_EQUAL(state.pieceAt(Square(7,4)), state.findThreatenedPiece(BLACK));
      TEST_CHECK_EQUAL(state.pieceAt(Square(3,6)), state.findThreatenedPiece(WHITE));
    }
    {
      EffectState state(csa::read_board(
                                     "P1-KY-KE-GI * -OU-KI *  * -KY\n"
                                     "P2 * -HI *  *  *  *  *  *  * \n"
                                     "P3 * -FU-FU-FU-FU-FU-FU-FU * \n"
                                     "P4 * +KA+KI+GI+KE *  *  *  * \n"
                                     "P5 *  *  *  *  *  *  *  *  * \n"
                                     "P6 *  *  *  * -KE-GI-KI-KA * \n"
                                     "P7 * +FU+FU+FU+FU+FU+FU+FU * \n"
                                     "P8 *  *  *  *  *  *  * +HI * \n"
                                     "P9+KY *  * +KI+OU * +GI+KE+KY\n"
                                     "P+00FU00FU00FU00FU\n"
                                     "+\n")); 
      TEST_CHECK_EQUAL(state.pieceAt(Square(8,4)), state.findThreatenedPiece(BLACK));
      TEST_CHECK_EQUAL(state.pieceAt(Square(2,6)), state.findThreatenedPiece(WHITE));
    }
    {
      EffectState state(csa::read_board(
                                     "P1-KY-KE-GI * -OU-KI *  * -KY\n"
                                     "P2 *  *  *  *  *  *  *  *  * \n"
                                     "P3 * -FU-FU-FU-FU-FU-FU-FU * \n"
                                     "P4+HI+KA+KI+GI+KE *  *  *  * \n"
                                     "P5 *  *  *  *  *  *  *  *  * \n"
                                     "P6 *  *  *  * -KE-GI-KI-KA-HI\n"
                                     "P7 * +FU+FU+FU+FU+FU+FU+FU * \n"
                                     "P8 *  *  *  *  *  *  *  *  * \n"
                                     "P9+KY *  * +KI+OU * +GI+KE+KY\n"
                                     "P+00FU00FU00FU00FU\n"
                                     "+\n")); 
      TEST_CHECK_EQUAL(state.pieceAt(Square(9,4)), state.findThreatenedPiece(BLACK));
      TEST_CHECK_EQUAL(state.pieceAt(Square(1,6)), state.findThreatenedPiece(WHITE));
    }
    {
      EffectState state(csa::read_board(
                                     "P1-KY-KE-GI * -OU-KI *  * -KY\n"
                                     "P2 *  *  *  *  *  *  *  *  * \n"
                                     "P3 * -FU-FU-FU-FU-FU-FU-FU * \n"
                                     "P4+UM+KA+KI+GI+KE *  *  *  * \n"
                                     "P5 *  *  *  *  *  *  *  *  * \n"
                                     "P6 *  *  *  * -KE-GI-KI-HI-RY\n"
                                     "P7 * +FU+FU+FU+FU+FU+FU+FU * \n"
                                     "P8 *  *  *  *  *  *  *  *  * \n"
                                     "P9+KY *  * +KI+OU * +GI+KE+KY\n"
                                     "P+00FU00FU00FU00FU\n"
                                     "+\n")); 
      TEST_CHECK_EQUAL(state.pieceAt(Square(9,4)), state.findThreatenedPiece(BLACK));
      TEST_CHECK_EQUAL(state.pieceAt(Square(1,6)), state.findThreatenedPiece(WHITE));
    }
  }
  {
    {
      EffectState state(csa::read_board(
                                     "P1-KY-KE-GI-KI-OU-KI-GI-KE-KY\n"
                                     "P2 * -HI *  *  *  *  * -KA * \n"
                                     "P3-FU-FU-FU-FU-FU-FU-FU-FU-FU\n"
                                     "P4 *  *  *  *  *  *  *  *  * \n"
                                     "P5 *  *  *  *  *  *  *  *  * \n"
                                     "P6 *  *  *  *  *  *  *  *  * \n"
                                     "P7+FU+FU+FU+FU+FU+FU+FU+FU+FU\n"
                                     "P8 * +KA *  *  *  *  * +HI * \n"
                                     "P9+KY+KE+GI+KI+OU+KI+GI+KE+KY\n"
                                     "+\n")); 
      TEST_CHECK(state.checkShadow(BLACK).none());
      TEST_CHECK(state.checkShadow(WHITE).none());
    }
    {
      EffectState state(csa::read_board(
                                     "P1-KY-KE-GI-KI-OU-KI-GI-KE-KY\n"
                                     "P2 * -HI *  *  *  *  *  *  * \n"
                                     "P3-FU-FU-FU-FU+TO-FU-FU-FU-FU\n"
                                     "P4 *  *  *  *  *  *  *  *  * \n"
                                     "P5 *  *  *  *  *  *  *  * -KA\n"
                                     "P6 *  *  *  * +KY *  *  *  * \n"
                                     "P7+FU+FU+FU+FU+FU+FU-TO+FU+FU\n"
                                     "P8 * +KA *  *  *  *  * +HI * \n"
                                     "P9+KY+KE+GI+KI+OU+KI+GI+KE * \n"
                                     "+\n")); 
      PieceMask bs=state.checkShadow(BLACK), ws=state.checkShadow(WHITE);
      TEST_CHECK_EQUAL(1, bs.countBit());
      TEST_CHECK_EQUAL(1, ws.countBit());

      TEST_CHECK_EQUAL(state[Square(5,3)], state.pieceOf(bs.takeOneBit()));
      TEST_CHECK_EQUAL(state[Square(3,7)], state.pieceOf(ws.takeOneBit()));
    }
  }

  {
    {
      EffectState state(csa::read_board(
                                     "P1-KY * +UM *  *  *  * -KE-KY\n"
                                     "P2 *  *  *  *  *  *  * -OU * \n"
                                     "P3 *  *  *  *  * -HI * -FU-FU\n"
                                     "P4-FU * -FU * -FU-KI-FU-GI * \n"
                                     "P5 *  *  *  *  *  *  *  * +FU\n"
                                     "P6+FU+FU+FU+KI+FU * +FU *  * \n"
                                     "P7 * +KI * +FU *  * -UM *  * \n"
                                     "P8 * +OU * +GI *  * -NG *  * \n"
                                     "P9+KY+KE *  *  *  *  *  * -RY\n"
                                     "P+00KI00GI00KY00FU\n"
                                     "P-00KE00KE00FU00FU00FU00FU\n"
                                     "+\n"));
      EffectState state2(csa::read_board(
                                      "P1+RY *  *  *  *  *  * -KE-KY\n"
                                      "P2 *  * +NG *  * -GI * -OU * \n"
                                      "P3 *  * +UM *  * -FU * -KI * \n"
                                      "P4 *  * -FU * -FU-KI-FU-FU-FU\n"
                                      "P5-FU *  *  *  *  *  *  *  * \n"
                                      "P6 * +GI+FU+KI+FU * +FU * +FU\n"
                                      "P7+FU+FU * +HI *  *  *  *  * \n"
                                      "P8 * +OU *  *  *  *  *  *  * \n"
                                      "P9+KY+KE *  *  *  * -UM * +KY\n"
                                      "P+00KE00KE00FU00FU00FU00FU\n"
                                      "P-00KI00GI00KY00FU\n"
                                      "-\n"));
      TEST_CHECK_EQUAL(state2, state.rotate180());
    }
  }

  {
    EffectState state(csa::read_board(
                                   "P1+NY+TO *  *  *  * -OU-KE-KY\n"
                                   "P2 *  *  *  *  * -GI-KI *  *\n"
                                   "P3 * +RY *  * +UM * -KI-FU-FU\n"
                                   "P4 *  * +FU-FU *  *  *  *  *\n"
                                   "P5 *  * -KE * +FU *  * +FU *\n"
                                   "P6-KE *  * +FU+GI-FU *  * +FU\n"
                                   "P7 *  * -UM *  *  *  *  *  *\n"
                                   "P8 *  *  *  *  *  *  *  *  * \n"
                                   "P9 * +OU * -GI *  *  *  * -NG\n"
                                   "P+00HI00KI00KE00KY00FU00FU00FU00FU00FU00FU\n"
                                   "P-00KI00KY00FU00FU\n"
                                   "P-00AL\n"
                                   "+\n"));

    // 後手の手
    TEST_CHECK(state.isLegal(Move(Square(6,9),Square(7,8),SILVER,Ptype_EMPTY,false,WHITE))==false);
    // 空白以外へのput
    TEST_CHECK(state.isLegal(Move(Square(2,1),PAWN,BLACK))==false);
    // 持っていない駒
    TEST_CHECK(state.isLegal(Move(Square(9,9),SILVER,BLACK))==false);
    // 二歩
    TEST_CHECK(state.isLegal(Move(Square(7,1),PAWN,BLACK))==false);
    // 二歩ではない
    TEST_CHECK(state.isLegal(Move(Square(8,2),PAWN,BLACK))==true);
    // 動けない場所
    TEST_CHECK(state.isLegal(Move(Square(4,1),PAWN,BLACK))==false);
    // fromにあるのがその駒か
    TEST_CHECK(state.isLegal(Move(Square(8,2),Square(7,3),PROOK,Ptype_EMPTY,false,BLACK))==false);
    // toにあるのが，相手の駒か空白か?
    TEST_CHECK(state.isLegal(Move(Square(8,1),Square(9,1),PPAWN,PLANCE,false,BLACK))==false);
    TEST_CHECK(state.isLegal(Move(Square(8,1),Square(7,1),PPAWN,Ptype_EMPTY,false,BLACK))==true);
    TEST_CHECK(state.isLegal(Move(Square(5,3),Square(4,2),PBISHOP,SILVER,false,BLACK))==true);

    // その offsetの動きがptypeに関してvalidか?
    TEST_CHECK(state.isLegal(Move(Square(8,1),Square(9,2),PPAWN,Ptype_EMPTY,false,BLACK))==false);
    // 離れた動きの時に間が全部空いているか?
    TEST_CHECK(state.isLegal(Move(Square(8,3),Square(6,3),PROOK,Ptype_EMPTY,false,BLACK))==true);
    TEST_CHECK(state.isLegal(Move(Square(8,3),Square(4,3),PROOK,Ptype_EMPTY,false,BLACK))==false);
    // capturePtypeが一致しているか?
    TEST_CHECK(state.isLegal(Move(Square(5,3),Square(4,2),PBISHOP,Ptype_EMPTY,false,BLACK))==false);
    // promoteしている時にpromote可能か
    TEST_CHECK(state.isLegal(Move(Square(8,1),Square(7,1),PPAWN,Ptype_EMPTY,true,BLACK))==false);
    // promoteしていない時に強制promoteでないか?
#if 0
    // 王手をかけられる
    // 現在のpawnMaskStateは判断できない
    TEST_CHECK(state.isLegal(Move(Square(8,9),Square(8,8),KING,Ptype_EMPTY,false,BLACK))==false);
#endif

    const EffectState s2(csa::read_board(
                                      "P1-KY *  *  *  *  *  * -KE-KY\n"
                                      "P2 *  *  *  *  *  * -KI * -OU\n"
                                      "P3-FU *  *  *  *  * -GI-GI * \n"
                                      "P4 *  * -FU * -FU * -FU-FU-FU\n"
                                      "P5 * -FU *  *  * +FU * +FU * \n"
                                      "P6 *  * +FU * +FU+KA+FU * +FU\n"
                                      "P7+FU * -TO *  * +KI+KE *  * \n"
                                      "P8+HI *  *  *  *  * +GI+OU * \n"
                                      "P9+KY *  * -RY * +KI *  * +KY\n"
                                      "P+00FU00FU00FU00KE00KI\n"
                                      "P-00KE00GI00KA\n"
                                      "+\n"));
    const Move illegal = Move(Square(2,5),Square(2,4),PPAWN,PAWN,true,BLACK);
    TEST_CHECK(! s2.isLegal(illegal));
  }
  {
    const char *state_false_string =
      "P1-OU-KI * +HI *  *  *  *  * \n"
      "P2+FU *  *  *  *  *  *  *  * \n"
      "P3 * +TO *  *  *  *  *  *  * \n"
      "P4 *  *  *  *  *  *  *  *  * \n"
      "P5 *  *  *  *  *  *  *  *  * \n"
      "P6 *  *  *  *  *  *  *  *  * \n"
      "P7 *  *  *  *  *  *  *  *  * \n"
      "P8 *  *  *  *  *  *  *  *  * \n"
      "P9 *  *  *  *  *  *  *  * +OU\n"
      "P+00FU\n"
      "P-00AL\n"
      "-\n";
    const EffectState state_false(csa::read_board(state_false_string));
    const Square target = Square(9,2);
    TEST_CHECK_EQUAL(Piece::EMPTY(), state_false.safeCaptureNotByKing
                     (WHITE, target));
  
    const char *state_true_string =
      "P1-OU-KI * +KA *  *  *  *  * \n"
      "P2+FU *  *  *  *  *  *  *  * \n"
      "P3 * +TO *  *  *  *  *  *  * \n"
      "P4 *  *  *  *  *  *  *  *  * \n"
      "P5 *  *  *  *  *  *  *  *  * \n"
      "P6 *  *  *  *  *  *  *  *  * \n"
      "P7 *  *  *  *  *  *  *  *  * \n"
      "P8 *  *  *  *  *  *  *  *  * \n"
      "P9 *  *  *  *  *  *  *  * +OU\n"
      "P+00FU\n"
      "P-00AL\n"
      "-\n";
    const EffectState state_true(csa::read_board(state_true_string));
    TEST_CHECK_EQUAL(state_true.pieceOnBoard(Square(8,1)), 
                     state_true.safeCaptureNotByKing
                     (WHITE, target));
  }
  {  
    const EffectState state(csa::read_board(
                                         "P1-KY *  *  * -OU * -FU *  * \n"
                                         "P2 *  * -KI * -KI-GI-KI *  * \n"
                                         "P3-FU-FU * -FU-FU-FU *  *  * \n"
                                         "P4 *  * +KE *  *  *  *  * -FU\n"
                                         "P5 * -KE *  *  *  *  *  *  * \n"
                                         "P6 * +FU *  *  *  *  *  *  * \n"
                                         "P7+FU * +FU+FU *  * +RY-NG+FU\n"
                                         "P8 * +GI+KI *  *  *  * +HI * \n"
                                         "P9+KY+KE *  *  * +KY * +OU+KY\n"
                                         "P+00KA00KA00GI00FU00FU\n"
                                         "P-00KE00FU00FU00FU00FU\n"
                                         "+\n"));
    TEST_CHECK_EQUAL(state[Square(2,8)], state.findCheapAttack(BLACK, Square(2,7)));
  }
}

void test_effect_state2()
{
  auto pieceNumConsistentBeforeCapture = [](EffectState& state) {
    TEST_CHECK(state.isConsistent());
    for (int i: all_piece_id()) {
        TEST_CHECK(state.isOnBoard(i));

        const Piece target = state.pieceOf(i);
        const Square pos = target.square();
        const Piece pieceOfSameLocation = state.pieceAt(pos);
        TEST_CHECK_EQUAL(target, pieceOfSameLocation);
      }
  };
  
  {
    EffectState s(csa::read_board(
                               "P1-KY-KE-GI-KI-OU-KI-GI-KE-KY\n"
                               "P2 * -HI *  *  *  *  * -KA * \n"
                               "P3-FU-FU-FU-FU-FU-FU-FU-FU-FU\n"
                               "P4 *  *  *  *  *  *  *  *  * \n"
                               "P5 *  *  *  *  *  *  *  *  * \n"
                               "P6 *  *  *  *  *  *  *  *  * \n"
                               "P7+FU+FU+FU+FU+FU+FU+FU+FU+FU\n"
                               "P8 * +KA *  *  *  *  * +HI * \n"
                               "P9+KY+KE+GI+KI+OU+KI+GI+KE+KY\n"
                               "+\n"));
    pieceNumConsistentBeforeCapture(s);
    s.makeMove(Move(Square(7,7),Square(7,6),PAWN,Ptype_EMPTY,false,BLACK));
    pieceNumConsistentBeforeCapture(s);
    s.makeMove(Move(Square(3,3),Square(3,4),PAWN,Ptype_EMPTY,false,WHITE));
    pieceNumConsistentBeforeCapture(s);
    const Move move = Move(Square(7,6),Square(7,5),PAWN,Ptype_EMPTY,false,BLACK);
    s.makeMove(move);
    const Piece expected = s.pieceAt(move.to());
    pieceNumConsistentBeforeCapture(s);
    for (int i=0; i<Piece::SIZE; ++i) {
      TEST_CHECK(s.isOnBoard(i));
      const Piece target = s.pieceOf(i);
      TEST_CHECK((target.square() != move.to())
                 || (expected == target));
      // 動いた駒自身でなければ，駒が重なることはない
    }
  }
  {
    EffectState s(csa::read_board(
                               "P1-KY-KE-GI-KI-OU-KI-GI-KE-KY\n"
                               "P2 * -HI *  *  *  *  * -KA * \n"
                               "P3-FU-FU-FU-FU-FU-FU-FU-FU-FU\n"
                               "P4 *  *  *  *  *  *  *  *  * \n"
                               "P5 *  *  *  *  *  *  *  *  * \n"
                               "P6 *  *  *  *  *  *  *  *  * \n"
                               "P7+FU+FU+FU+FU+FU+FU+FU+FU+FU\n"
                               "P8 * +KA *  *  *  *  * +HI * \n"
                               "P9+KY+KE+GI+KI+OU+KI+GI+KE+KY\n"
                               "+\n"));
    s.makeMove(Move(Square(7,7),Square(7,6),PAWN,Ptype_EMPTY,false,BLACK));
    s.makeMove(Move(Square(3,3),Square(3,4),PAWN,Ptype_EMPTY,false,WHITE));
    TEST_CHECK(s.isConsistent());

    const Square from = Square(8,8);
    const Square to = Square(2,2);
    const Move move = Move(from,to,PBISHOP,BISHOP,true,BLACK);
    TEST_CHECK(s.isLegal(move));
    s.makeMove(move);
    TEST_CHECK(s.isConsistent());
    const Piece expected = s.pieceAt(move.to());
    for (int i=0; i<Piece::SIZE; ++i) {
      if(s.isOnBoard(i)){
        const Piece target = s.pieceOf(i);
        TEST_CHECK((target.square() != move.to())
                   || (expected == target)); // 動いた駒自身でなければ，駒が重なることはない
      }
    }
  }
  {
    {
      const EffectState state(csa::read_board(
                                           "P1 *  *  *  *  *  *  * -OU * \n"
                                           "P2 *  *  *  *  *  *  * +KI * \n"
                                           "P3 *  *  *  *  *  *  * +FU * \n"
                                           "P4 *  *  *  *  *  *  *  *  * \n"
                                           "P5 *  *  *  *  *  *  *  *  * \n"
                                           "P6 *  *  *  *  *  *  *  *  * \n"
                                           "P7 *  *  *  *  *  *  *  *  * \n"
                                           "P8 *  *  *  *  *  *  *  *  * \n"
                                           "P9 *  * +OU *  *  *  *  *  * \n"
                                           "P+00KI\n"
                                           "P-00AL\n"
                                           "-\n"));
      TEST_CHECK(state.inUnblockableCheck(WHITE));
      TEST_CHECK(! state.inUnblockableCheck(BLACK));
    }
    {
      const EffectState state(csa::read_board(
                                           "P1 *  *  *  *  * +HI * -OU * \n"
                                           "P2 *  *  *  *  *  *  * +GI * \n"
                                           "P3 *  *  *  *  *  *  * +FU * \n"
                                           "P4 *  *  *  *  *  *  *  *  * \n"
                                           "P5 *  *  *  *  *  *  *  *  * \n"
                                           "P6 *  * -KY *  *  *  *  *  * \n"
                                           "P7 *  *  *  *  *  *  *  *  * \n"
                                           "P8 *  *  *  *  *  *  *  *  * \n"
                                           "P9 *  * +OU *  *  *  *  *  * \n"
                                           "P+00KI\n"
                                           "P-00AL\n"
                                           "-\n"));
      TEST_CHECK(state.inUnblockableCheck(WHITE));
      TEST_CHECK(! state.inUnblockableCheck(BLACK));
    }
    {
      const EffectState state(csa::read_board(
                                           "P1 *  *  *  *  * +HI * -OU * \n"
                                           "P2 *  *  *  *  *  *  * +KA * \n"
                                           "P3 *  *  *  *  *  *  * +FU * \n"
                                           "P4 *  *  *  *  *  *  *  *  * \n"
                                           "P5 *  *  *  *  *  *  *  *  * \n"
                                           "P6 *  * -KY *  *  *  *  *  * \n"
                                           "P7 *  *  *  *  *  *  *  *  * \n"
                                           "P8 *  *  * -NG *  *  *  *  * \n"
                                           "P9 *  * +OU *  *  *  *  *  * \n"
                                           "P+00KI\n"
                                           "P-00AL\n"
                                           "-\n"));
      TEST_CHECK(! state.inUnblockableCheck(WHITE));
      TEST_CHECK(state.inUnblockableCheck(BLACK));
    }
  }

  {
    {
      EffectState state1(csa::read_board(
                                      "P1-KY+RY *  *  *  *  *  * -KY\n"
                                      "P2 *  *  *  * +UM * +NK *  * \n"
                                      "P3-FU-OU-GI-FU-FU-FU *  * -FU\n"
                                      "P4 *  * -FU *  *  *  *  *  * \n"
                                      "P5 *  *  *  * +KA *  *  *  * \n"
                                      "P6 *  *  *  *  *  *  *  *  * \n"
                                      "P7+FU * +FU+FU+FU+FU+FU * +FU\n"
                                      "P8 *  * -NK * +OU *  *  *  * \n"
                                      "P9+KY+KE * -HI * +KI+GI * +KY\n"
                                      "P-00FU\n"
                                      "P+00FU\n"
                                      "P+00FU\n"
                                      "P-00FU\n"
                                      "P-00FU\n"
                                      "P+00KE\n"
                                      "P-00GI\n"
                                      "P-00GI\n"
                                      "P+00KI\n"
                                      "P-00KI\n"
                                      "P-00KI\n"
                                      "-\n"
                                      ));
      EffectState sState(state1);
      // 8-4 には8-3を取り除くと先手の利きがある．
      TEST_CHECK((sState.
                  hasEffectByWithRemove<BLACK>(Square(8,4),Square(8,3))));
      // 8-9 には8-3を取り除くと先手の利きがある．
      TEST_CHECK((sState.
                  hasEffectByWithRemove<BLACK>(Square(8,9),Square(8,3))));
    }
    {
      EffectState state1(csa::read_board(
                                      "P1-KY-KE+GI+KA *  * +RY * -KY\n"
                                      "P2 *  * -OU * -KI * +NK *  * \n"
                                      "P3-FU * -GI-FU-FU-FU *  * -FU\n"
                                      "P4 *  * -FU *  *  *  *  *  * \n"
                                      "P5 *  *  *  * +KA *  *  *  * \n"
                                      "P6 *  *  *  *  *  *  *  *  * \n"
                                      "P7+FU * +FU+FU+FU+FU+FU * +FU\n"
                                      "P8 *  * -NK * +OU *  *  *  * \n"
                                      "P9+KY+KE * -HI * +KI+GI * +KY\n"
                                      "P+00FU\n"
                                      "P+00FU\n"
                                      "P+00FU\n"
                                      "P-00FU\n"
                                      "P-00FU\n"
                                      "P-00GI\n"
                                      "P-00KI\n"
                                      "P-00KI\n"
                                      "-\n"
                                      ));
      EffectState sState(state1);
      // 8-3 には7-2を取り除くと先手の利きがある．
      TEST_CHECK((sState.
                  hasEffectByWithRemove<BLACK>(Square(8,3),Square(7,2))));
    }
  }
}

void test_usi() {
  {
    EffectState state;
    const Move m76fu = Move(Square(7,7), Square(7,6), PAWN, 
                            Ptype_EMPTY, false, BLACK);
    const std::string usi76fu = "7g7f";
    const std::string usi76fu2 = to_usi(m76fu);
    TEST_CHECK_EQUAL(usi76fu, usi76fu2);

    const Move m76fu2 = usi::to_move(usi76fu, state);
    TEST_CHECK_EQUAL(m76fu, m76fu2);

    const std::string usi_win = "win";
    const Move win = usi::to_move(usi_win, state);
    TEST_CHECK_EQUAL(win, Move::DeclareWin());
  }
  {
    EffectState state;
    const std::string hirate = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL";

    EffectState state2;
    usi::parse_board(hirate, state2);
    TEST_CHECK_EQUAL(static_cast<const SimpleState&>(state), state2);
  }
  {
    EffectState state;
    const std::string hirate = "8l/1l+R2P3/p2pBG1pp/kps1p4/Nn1P2G2/P1P1P2PP/1PS6/1KSG3+r1/LN2+p3L";

    EffectState state2;
    TEST_CHECK((usi::parse_board(hirate, state2), true));
  }
  {
    {
      EffectState state(csa::read_board(
                                     "P1-KY-KE-GI-KI-OU-KI-GI-KE-KY\n"
                                     "P2 * -HI *  *  *  *  * -KA * \n"
                                     "P3-FU-FU-FU-FU-FU-FU * -FU-FU\n"
                                     "P4 *  *  *  *  *  * -FU *  * \n"
                                     "P5 *  *  *  *  *  *  *  *  * \n"
                                     "P6 *  * +FU *  *  *  * +FU * \n"
                                     "P7+FU+FU * +FU+FU+FU+FU * +FU\n"
                                     "P8 * +KA *  *  *  *  * +HI * \n"
                                     "P9+KY+KE+GI+KI+OU+KI+GI+KE+KY\n"
                                     "-\n"));

      const std::string s763426 = "startpos moves 7g7f 3c3d 2g2f";

      EffectState state2;
      usi::parse(s763426, state2);
      TEST_CHECK_EQUAL(static_cast<const SimpleState&>(state), state2);
    }
    {
      EffectState state(csa::read_board(
                                     "P1-KY-KE-GI-KI-OU-KI-GI-KE-KY\n"
                                     "P2 * -HI *  *  *  *  *  *  * \n"
                                     "P3 *  *  * -FU-FU-FU-FU-FU-FU\n"
                                     "P4 *  *  *  *  *  *  *  *  * \n"
                                     "P5 *  *  *  *  *  *  *  *  * \n"
                                     "P6 *  *  *  *  *  *  *  *  * \n"
                                     "P7+FU+FU+FU+FU+FU+FU+FU *  * \n"
                                     "P8 * +KA *  *  *  *  * +HI * \n"
                                     "P9+KY+KE * +KI+OU+KI+GI+KE+KY\n"
                                     "P+00GI00FU00FU\n"
                                     "P-00KA00FU00FU00FU\n"
                                     "-\n"));

      const std::string stest = "sfen lnsgkgsnl/1r7/3pppppp/9/9/9/PPPPPPP2/1B5R1/LN1GKGSNL w S2Pb3p 1";

      EffectState state2;
      usi::parse(stest, state2);
      TEST_CHECK_EQUAL(static_cast<const SimpleState&>(state), state2);

      const std::string stest3 = "sfen lnsgkgsnl/1r7/3pppppp/9/9/9/PPPPPPP2/1B5R1/LN1GKGSNL w S2Pb3p";
      EffectState state3;
      usi::parse(stest3, state3);
      TEST_CHECK_EQUAL(static_cast<const SimpleState&>(state), state3);
    }
  }
  {
    EffectState state;

    TEST_EXCEPTION(usi::parse_board("lnsgkgsna/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL", state), // invalid ascii character a
                   osl::usi::ParseError);
    TEST_EXCEPTION(usi::parse_board("", state), // empty
                      osl::usi::ParseError);
    TEST_EXCEPTION(usi::parse_board("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL+", state), // ends with +
                      osl::usi::ParseError);
    TEST_EXCEPTION(usi::parse_board("8l/1l+R2P3/p2pBG1pp/kps1p4/Nn1P2G2/P1P1P2PP/1PS6/1KSG3+r1/LN2+3L", state), // should be ...+p3L
                      osl::usi::ParseError);
    TEST_EXCEPTION(usi::parse_board("lnsg+kgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL", state), // k can not be promoted
                      osl::usi::ParseError);
    TEST_EXCEPTION(usi::parse_board("lnsgkgsnl/1r5b2/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL", state), // too many columns at the second row
                      osl::usi::ParseError);
    TEST_EXCEPTION(usi::parse_board("lnsgkgsnl/1r5b1/pppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL", state), // too many columns at the third row
                      osl::usi::ParseError);
    TEST_EXCEPTION(usi::parse_board("lnsgkgsnl/1r5b10/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL", state), // 0 is invalid
                      osl::usi::ParseError);
    TEST_EXCEPTION(usi::parse_board("lnsgkgsn?/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL", state), // ? is invalid
                      osl::usi::ParseError);
  }
  {
    EffectState state;

    TEST_EXCEPTION(usi::parse("lnsgkgsnl/1r7/3pppppp/9/9/9/PPPPPPP2/1B5R1/LN1GKGSNL w S2Pb3p 1", state), // should start with sfen
                      osl::usi::ParseError);
    TEST_EXCEPTION(usi::parse("sfen lnsgkgsnl/1r7/3pppppp/9/9/9/PPPPPPP2/1B5R1/LN1GKGSNL l S2Pb3p 1", state), // should be black or white
                      osl::usi::ParseError);
    TEST_EXCEPTION(usi::parse("sfen lnsgkgsnl/1r7/3pppppp/9/9/9/PPPPPPP2/1B5R1/LN1GKGSNL w S2Ab3p 1", state), // invalid ascii character in hands
                      osl::usi::ParseError);
    TEST_EXCEPTION(usi::parse("sfen lnsgkgsnl/1r7/3pppppp/9/9/9/PPPPPPP2/1B5R1/LN1GKGSNL w S2#b3p 1", state), // invalid character in hands
                      osl::usi::ParseError);
    TEST_EXCEPTION(usi::parse("sfen lnsgkgsnl/1r7/3pppppp/9/9/9/PPPPPPP2/1B5R1/LN1GKGSNL w S0Pb3p 1", state), // 0 is invalid
                      osl::usi::ParseError);
    TEST_CHECK((usi::parse("sfen lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - a", state), true)); // invalid moves are not respected
  }
  {
    EffectState state;
    usi::parse("sfen 3l+R4/1l1g5/2k1n4/1p+p2s3/4+r+B3/1G1s+p4/9/2N2+p3/9 b BGS2NPgs2l13p 1", state);
    usi::parse("sfen 6n2/+PS1+Ps1k2/3s+B1l2/1Lp1G1p1S/3p1n1+R+p/3+p1+nG2/4Pp1G1/6G+r1/5L1N+B b 2Pl7p 1", state);
  }
  {
    {
      EffectState state;
      TEST_CHECK_EQUAL(std::string("startpos"), to_usi(state));
    }
    {
      std::string str = "sfen kng5l/ls1S2r2/pppPp2Pp/7+B1/7p1/2Pp2P2/PP3P2P/L2+n5/K1G5L b B2S2N2Pr2gp 1";
      EffectState state(csa::read_board(
                                     "P1-OU-KE-KI *  *  *  *  * -KY\n"
                                     "P2-KY-GI * +GI *  * -HI *  * \n"
                                     "P3-FU-FU-FU+FU-FU *  * +FU-FU\n"
                                     "P4 *  *  *  *  *  *  * +UM * \n"
                                     "P5 *  *  *  *  *  *  * -FU * \n"
                                     "P6 *  * +FU-FU *  * +FU *  * \n"
                                     "P7+FU+FU *  *  * +FU *  * +FU\n"
                                     "P8+KY *  * -NK *  *  *  *  * \n"
                                     "P9+OU * +KI *  *  *  *  * +KY\n"
                                     "P+00KA00GI00GI00KE00KE00FU00FU\n"
                                     "P-00HI00KI00KI00FU\n"
                                     "+"));
      TEST_CHECK_EQUAL(str, to_usi(state));
      EffectState test;
      usi::parse(str, test);
      TEST_CHECK_EQUAL(static_cast<const SimpleState&>(state), test);
    }
    {
      std::string str = "sfen lnsg4l/2k2R+P2/pppp1s2p/9/4p1P2/2P2p+n2/PP1P4P/2KSG1p2/LN1G4L w BG2Prbsnp 1";
      EffectState state(csa::read_board(
                                     "P1-KY-KE-GI-KI *  *  *  * -KY\n"
                                     "P2 *  * -OU *  * +HI+TO *  * \n"
                                     "P3-FU-FU-FU-FU * -GI *  * -FU\n"
                                     "P4 *  *  *  *  *  *  *  *  * \n"
                                     "P5 *  *  *  * -FU * +FU *  * \n"
                                     "P6 *  * +FU *  * -FU-NK *  * \n"
                                     "P7+FU+FU * +FU *  *  *  * +FU\n"
                                     "P8 *  * +OU+GI+KI * -FU *  * \n"
                                     "P9+KY+KE * +KI *  *  *  * +KY\n"
                                     "P+00KA00KI00FU00FU\n"
                                     "P-00HI00KA00GI00KE00FU\n"
                                     "-\n"));
      TEST_CHECK_EQUAL(str, to_usi(state));
      EffectState test;
      usi::parse(str, test);
      TEST_CHECK_EQUAL(static_cast<const SimpleState&>(state), test);
    }
  }
  {
    {
      const std::string sfen = "position sfen lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1 moves 5a6b 2g2f 6b7b 2f2e 7b8b 2e2d 2c2d 2h2d";
      EffectState state;
      std::vector<Move> moves;
      TEST_CHECK((usi::parse(sfen, state, moves), true)); // no throw

      for (Ptype ptype: PieceStand::order) {
        TEST_CHECK_EQUAL(0, state.countPiecesOnStand(BLACK, ptype));
        TEST_CHECK_EQUAL(0, state.countPiecesOnStand(WHITE, ptype));
      }

      for (Move move: moves) {
        MoveVector all;
        state.generateLegal(all);
        TEST_CHECK(is_member(all, move));
        state.makeMove(move);

        for (int i = 0; i < Piece::SIZE; ++i) {
          if (! state.usedMask().test(i)) {
            TEST_CHECK(state.pieceOf(i).owner() == WHITE);
            TEST_CHECK(! state.pieceOf(i).isOnBoard());
          }
        }
      }
    }
    {
      const std::string sfen = "position sfen lnsg1gsnl/1k7/ppppppp1p/7R1/9/9/PPPPPPP1P/1B7/LNSGKGSNL w Pp 1";
      EffectState state;
      TEST_CHECK((usi::parse(sfen, state), true));
    }
  }
}

void test_classify()
{
  // check
  {
    const char *stateString = 
      "P1-KY *  *  *  *  *  * +NY * \n"
      "P2 * -OU-KI-KI *  *  *  * +RY\n"
      "P3 * -GI-KE+KI *  *  *  * +HI\n"
      "P4 *  * -FU-KY-FU *  * -FU * \n"
      "P5-FU-FU * -KE * -FU *  *  * \n"
      "P6 *  * +FU-FU+FU * -FU *  * \n"
      "P7+FU+FU *  *  *  *  *  *  * \n"
      "P8+KY+GI+GI-UM *  *  *  *  * \n"
      "P9+OU+KE *  *  *  *  * +KE * \n"
      "P-00FU\n"
      "P-00FU\n"
      "P-00FU\n"
      "P-00FU\n"
      "P-00FU\n"
      "P-00FU\n"
      "P-00GI\n"
      "P+00KI\n"
      "P-00KA\n"
      "+\n";
    EffectState state((csa::read_board(stateString)));
    const Move m = Move(Square(7,1),GOLD,BLACK);
    TEST_CHECK_EQUAL(false, is_pawn_drop_checkmate(state, m));
  }
  // kingOpen
  using osl::move_classifier::KingOpenMove;
  {
    // 王手がかかった状態での開き王手
    EffectState state=csa::read_board(
                                   "P1+NY+RY *  *  * -FU-OU-KE-KY\n"
                                   "P2 *  *  *  *  * +GI-KI *  *\n"
                                   "P3 * -FU *  *  *  * -KI-FU-FU\n"
                                   "P4 * +FU * -FU *  *  *  *  *\n"
                                   "P5 *  * -KE * +FU *  * +FU *\n"
                                   "P6+KE *  * +FU+GI *  *  * +FU\n"
                                   "P7 *  * -UM-KA *  *  *  *  *\n"
                                   "P8 *  * +FU *  *  *  *  *  * \n"
                                   "P9 * +OU * -GI *  *  *  * -NG\n"
                                   "P+00HI00KI00KE00KY00FU00FU00FU00FU00FU\n"
                                   "P-00KI00KY00FU00FU\n"
                                   "P-00AL\n"
                                   "-\n"
                                   );
    // 開き王手ではない
    TEST_CHECK(!(KingOpenMove::isMember<WHITE>(state,GOLD,Square(3,2),Square(4,2))));
    // 開き王手
    TEST_CHECK((KingOpenMove::isMember<WHITE>(state,PAWN,Square(4,1),Square(4,2))));
  }
  {
    // 
    EffectState state=csa::read_board(
                                   "P1+NY+RY *  *  * -FU-OU-KE-KY\n"
                                   "P2 *  *  *  *  *  * -KI *  *\n"
                                   "P3 * -FU *  *  * +GI-KI-FU-FU\n"
                                   "P4 * +FU * -FU *  *  *  *  *\n"
                                   "P5 *  * -KE * +FU-KA * +FU *\n"
                                   "P6+KE * +FU+FU+GI *  *  * +FU\n"
                                   "P7 *  * -UM *  *  *  *  *  *\n"
                                   "P8 *  *  *  *  *  *  *  *  * \n"
                                   "P9 * +OU * -GI *  *  *  * -NG\n"
                                   "P+00HI00KI00KE00KY00FU00FU00FU00FU00FU\n"
                                   "P-00KI00KY00FU00FU\n"
                                   "P-00AL\n"
                                   "+\n"
                                   );
    // 開き王手ではない
    TEST_CHECK(!(KingOpenMove::isMember<BLACK>(state,SILVER,Square(5,6),Square(6,7))));
  }
  // safeMove
  using osl::move_classifier::SafeMove;
  {
    EffectState state=csa::read_board(
                                   "P1+NY+TO *  *  *  * -OU-KE-KY\n"
                                   "P2 *  *  *  *  * -GI-KI *  *\n"
                                   "P3 * -RY *  *  *  * -KI-FU-FU\n"
                                   "P4 * +FU * -FU *  *  *  *  *\n"
                                   "P5 *  * -KE * +FU *  * +FU *\n"
                                   "P6+KE *  * +FU+GI-FU *  * +FU\n"
                                   "P7 *  * -UM-KA *  *  *  *  *\n"
                                   "P8 *  * +FU *  *  *  *  *  * \n"
                                   "P9 * +OU * -GI *  *  *  * -NG\n"
                                   "P+00HI00KI00KE00KY00FU00FU00FU00FU00FU\n"
                                   "P-00KI00KY00FU00FU\n"
                                   "P-00AL\n"
                                   "+\n"
                                   );
    // 王が動くが安全
    TEST_CHECK((SafeMove::isMember<BLACK>(state,KING,Square(8,9),Square(7,9))));
    // 王が動いて危険
    TEST_CHECK((!SafeMove::isMember<BLACK>(state,KING,Square(8,9),Square(8,8))));
    // 龍をblockしている歩が動くが相手の龍を取るので安全
    TEST_CHECK((SafeMove::isMember<BLACK>(state,PAWN,Square(8,4),Square(8,3))));
    // 角をblockしている歩が動いて危険
    TEST_CHECK(!(SafeMove::isMember<BLACK>(state,PAWN,Square(7,8),Square(7,7))));
  }
  {
    EffectState state=csa::read_board(
                                   "P1-KY-KE-GI-KI-OU-KI-GI-KE-KY\n"
                                   "P2 * -HI *  *  *  *  * -KA * \n"
                                   "P3-FU-FU-FU-FU-FU-FU-FU-FU-FU\n"
                                   "P4 *  *  *  *  *  *  *  *  * \n"
                                   "P5 *  *  *  *  *  *  *  *  * \n"
                                   "P6 *  * +FU *  *  *  *  *  * \n"
                                   "P7+FU+FU * +FU+FU+FU+FU+FU+FU\n"
                                   "P8 * +KA *  *  *  *  * +HI * \n"
                                   "P9+KY+KE+GI+KI+OU+KI+GI+KE+KY\n"
                                   "-\n"
                                   );
    // 安全
    TEST_CHECK((SafeMove::isMember<WHITE>(state,PAWN,Square(4,3),Square(4,4))));
  }
  {
    EffectState state=csa::read_board(
                                   "P1-KY-KE * -KI-OU * -GI+RY-KY\n"
                                   "P2 *  *  *  *  *  * -KI *  * \n"
                                   "P3-FU * -GI-FU-FU-FU *  * -FU\n"
                                   "P4 *  * -FU *  *  *  *  *  * \n"
                                   "P5 *  *  *  * +KA *  *  *  * \n"
                                   "P6 *  *  *  *  *  *  *  *  * \n"
                                   "P7+FU * +FU+FU+FU+FU+FU * +FU\n"
                                   "P8 * +GI+KI *  *  *  *  *  * \n"
                                   "P9+KY+KE *  * +OU+KI+GI * +KY\n"
                                   "P+00FU\n"
                                   "P+00FU\n"
                                   "P-00FU\n"
                                   "P-00FU\n"
                                   "P-00FU\n"
                                   "P+00KE\n"
                                   "P-00KE\n"
                                   "P-00KA\n"
                                   "P-00HI\n"
                                   "-\n"
                                   );
    // 安全ではない
    TEST_CHECK(!(SafeMove::isMember<WHITE>(state,SILVER,Square(3,1),Square(2,2))));
  }
  {
    EffectState state=csa::read_board(
                                   "P1 *  *  *  *  *  *  *  * -KY\n"
                                   "P2 *  *  * +TO * -GI *  * -GI\n"
                                   "P3-OU+FU *  *  * -KI *  * -FU\n"
                                   "P4 *  *  * -GI-FU * -FU-FU * \n"
                                   "P5 * -HI *  *  *  *  *  *  * \n"
                                   "P6 *  * +OU+FU+FU * +FU *  * \n"
                                   "P7+FU *  *  * +KI *  *  * +FU\n"
                                   "P8 * -NY * -NK *  *  *  *  * \n"
                                   "P9+KY *  *  *  *  *  * +KE+KY\n"
                                   "P+00FU00FU\n"
                                   "P-00HI00KA00KA00KI00KI00GI00KE00KE00FU00FU00FU00FU00FU\n"
                                   "+\n");
    TEST_CHECK(!(SafeMove::isMember<BLACK>(state,KING,Square(7,6),Square(8,6))));    
  }
}

void test_movegen() {
  {
    auto state = csa::read_board("P1-KY-KE-GI *  *  *  * -KE-KY\n"
                                 "P2 * -OU-KI *  *  *  *  *  * \n"
                                 "P3 * -FU-FU-KI * -GI * -FU-FU\n"
                                 "P4 *  *  * -FU *  * -FU+FU * \n"
                                 "P5+FU *  *  * +FU-FU *  *  * \n"
                                 "P6 *  * +FU *  *  * +FU+HI * \n"
                                 "P7 * +FU+KE *  * +FU *  * +FU\n"
                                 "P8 *  * +KI+GI+KI *  *  *  * \n"
                                 "P9+KY+OU+GI *  *  * -UM+KE+KY\n"
                                 "P+00HI00FU00FU\n"
                                 "P-00KA00FU\n"
                                 "-\n");
      MoveVector moves;
      state.generateLegal(moves);
      Move m97fu = Move(Square(9,7), PAWN, WHITE);
      TEST_ASSERT(is_member(moves, m97fu));
  }
  {
    const EffectState state(csa::read_board("P1-KY *  * -KI *  *  * -KE-KY\n"
                                               "P2 * -OU-GI *  * -HI * -KA * \n"
                                               "P3 *  * -KE-KI-FU *  * -FU * \n"
                                               "P4 * -FU-FU-FU-GI * -FU * -FU\n"
                                               "P5-FU *  *  *  * -FU * +KE * \n"
                                               "P6 *  * +FU * +GI * +FU+FU+FU\n"
                                               "P7+FU+FU+KA+FU+FU *  *  *  * \n"
                                               "P8+KY * +KI+KI *  *  *  *  * \n"
                                               "P9+OU+KE+GI *  * +HI *  * +KY\n"
                                               "P-00FU\n"
                                               "+\n"));
    {
      MoveVector moves;
      state.generateLegal(moves);
      Move m22UM = Move(Square(7,7), Square(2,2), PBISHOP, BISHOP, true,  BLACK);
      Move m22KA = Move(Square(7,7), Square(2,2), BISHOP,  BISHOP, false, BLACK);
      TEST_CHECK(is_member(moves, m22UM));
      TEST_CHECK(! is_member(moves, m22KA));
    }
    {
      MoveVector moves;
      state.generateWithFullUnpromotions(moves);
      Move m22UM = Move(Square(7,7), Square(2,2), PBISHOP, BISHOP, true,  BLACK);
      Move m22KA = Move(Square(7,7), Square(2,2), BISHOP,  BISHOP, false, BLACK);
      TEST_CHECK(is_member(moves, m22UM));
      TEST_CHECK(is_member(moves, m22KA));
    }
  }

  {
    EffectState state(csa::read_board(
                                         "P1-KY-KE *  *  *  *  *  * -KY\n"
                                         "P2 * +HI-GI *  *  * -KI-GI * \n"
                                         "P3-FU+TO-OU+FU-FU-FU-KE * -FU\n"
                                         "P4 *  *  * -FU *  *  * -FU * \n"
                                         "P5 *  *  *  *  *  * -FU *  * \n"
                                         "P6 *  *  *  *  * +FU *  * +FU\n"
                                         "P7+FU * -TO * +FU * +FU+FU * \n"
                                         "P8 * -HI *  *  *  * +GI+OU * \n"
                                         "P9+KY+KE * +KI * +KI * +KE+KY\n"
                                         "P+00KA\n"
                                         "P-00KA00KI00GI00FU00FU\n"
                                         "-\n"));
    MoveVector moves;
    state.generateWithFullUnpromotions(moves);
    for (Move move: moves) {
      TEST_CHECK_EQUAL(1, std::count(moves.begin(), moves.end(), move));
    }
  }
  
}

static bool is_checkmated(const EffectState &state)
{
  Player pl=state.turn();
  // 自殺手
  if(state.hasEffectAt(pl,state.kingSquare(alt(pl)))) return false;
  // not 王手
  if(!state.hasEffectAt(alt(pl),state.kingSquare(pl))) return false;
  MoveVector moves;
  state.generateLegal(moves);

  for(Move move: moves){
    EffectState next_state = state;
    next_state.makeMove(move);
    Player pl=next_state.turn();
    // 王手をのがれた
    if(!next_state.hasEffectAt(pl,next_state.kingSquare(alt(pl)))) return false;
  }
  return true;
}

template<bool onlyDrop>
static bool isImmediateCheck(const EffectState& state_org,Move const& move)
{
  if(onlyDrop && move.from().isOnBoard()) return false;
  if(!move.from().isOnBoard() && move.ptype()==PAWN) return false;
  EffectState state = state_org;
  state.makeMove(move);
  if(is_checkmated(state))
    return true;
  return false;
}
void test_checkmate()
{
  {
    EffectState state
      (csa::read_board(
	"P1 *  *  *  *  *  *  * -KE-OU\n"
	"P2 *  *  *  *  *  *  * -KE * \n"
	"P3 *  *  *  *  *  *  *  *  * \n"
	"P4 *  *  *  *  *  *  * +KE * \n"
	"P5 *  *  *  *  *  *  *  *  * \n"
	"P6 *  *  *  *  *  *  *  *  * \n"
	"P7 *  *  *  *  *  *  *  *  * \n"
	"P8 *  *  *  *  *  *  *  *  * \n"
	"P9 * +OU *  *  *  *  *  *  * \n"
	"P+00FU\n"
	"P-00AL\n"
	"+\n"));
    Move best_move;
    const bool checkmate 
      = ImmediateCheckmate::hasCheckmateMove(BLACK,state,best_move);
    if (checkmate)
      std::cerr << best_move << "\n";
    TEST_CHECK(! checkmate);
  }
  {
    EffectState state
      (csa::read_board(
	"P1 *  *  *  *  *  *  * -KE-OU\n"
	"P2 *  *  *  *  *  *  * -KE * \n"
	"P3 *  *  *  *  *  *  *  *  * \n"
	"P4 *  *  *  *  *  *  * +KE * \n"
	"P5 *  *  *  *  *  *  *  *  * \n"
	"P6 *  *  *  *  *  *  *  *  * \n"
	"P7 *  *  *  *  *  *  *  *  * \n"
	"P8 *  *  *  *  *  *  *  *  * \n"
	"P9 * +OU *  *  *  *  *  *  * \n"
	"P+00FU00KY\n"
	"P-00AL\n"
	"+\n"));
    Move best_move;
    const bool checkmate 
      = ImmediateCheckmate::hasCheckmateMove(BLACK,state,best_move);
    TEST_CHECK(checkmate);
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY-KE *  *  * +HI-FU-OU-KY\n"
				 "P2 *  *  *  *  *  *  *  *  * \n"
				 "P3-FU * -FU * -FU-GI+TO *  * \n"
				 "P4 *  *  *  *  *  *  * -FU * \n"
				 "P5 * -FU * +FU-KA+KI *  * -FU\n"
				 "P6 *  * +FU *  *  *  * +FU * \n"
				 "P7+FU+FU *  *  * +KI *  * +FU\n"
				 "P8 *  *  *  *  *  *  * +HI+KY\n"
				 "P9 * +KE *  *  *  * -UM+KE+OU\n"
				 "P-00KI00KI00GI00GI00GI00KE00KY00FU00FU00FU00FU\n"
				 "-\n"
                         ));
    // +0027KE or +5528UM
    Move bestMove;
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(WHITE,state,bestMove));
    TEST_CHECK(Move(Square(2,7),KNIGHT,WHITE) ==  bestMove ||
		   Move(Square(5,5),Square(2,8),PBISHOP,ROOK,true,WHITE) ==  bestMove);
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY *  *  *  *  *  * -KE-KY\n"
				 "P2 *  *  *  * +RY *  *  *  * \n"
				 "P3 * +NK * +TO *  * -KI-FU * \n"
				 "P4 * +GI *  *  *  * -FU * -FU\n"
				 "P5-FU *  *  *  *  *  * -RY * \n"
				 "P6 *  *  * +OU-KA-KI *  * -OU\n"
				 "P7+FU-UM *  * +KI *  *  *  * \n"
				 "P8 *  *  *  * +GI *  *  * -KI\n"
				 "P9 *  *  *  *  *  *  *  *  * \n"
				 "P+00GI00GI00FU00FU\n"
				 "P-00KE00KE00KY00KY00FU00FU00FU00FU00FU00FU00FU00FU00FU00FU\n"
				 "-\n"
                                   ));
    // -0074KE
    Move bestMove;
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(WHITE,state,bestMove));

    TEST_CHECK_EQUAL(Move(Square(2,5),Square(6,5),PROOK,Ptype_EMPTY,false,WHITE),bestMove);
  }
  {
    EffectState state(csa::read_board(
				 "P1 * +RY * -KI-OU *  * -KE-KY\n"
				 "P2+KA *  *  *  *  * -KI *  * \n"
				 "P3 *  *  * -FU-FU-FU-GI-FU-FU\n"
				 "P4 *  * -FU+KE *  *  *  *  * \n"
				 "P5 * +GI+FU *  *  *  * +FU * \n"
				 "P6 *  *  *  * +FU+FU *  *  * \n"
				 "P7 *  * -RY+FU+OU+GI+FU * +FU\n"
				 "P8 *  *  *  *  *  *  *  *  * \n"
				 "P9+KY-GI *  *  * +KI * +KE+KY\n"
				 "P+00KA00KI00KY00FU00FU\n"
				 "P-00KE00FU00FU00FU\n"
				 "+\n"
                                   )); 
    Move bestMove;
    // +0052KIは普通のpinned
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(BLACK,state,bestMove));
    TEST_CHECK(bestMove == Move(Square(5,2),GOLD,BLACK));
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY-KE+RY-KI-OU-KI * -RY * \n"
				 "P2 *  *  *  *  *  *  *  *  * \n"
				 "P3-FU-FU-FU-FU *  *  *  * -FU\n"
				 "P4 *  *  *  * +KE-KI-FU *  * \n"
				 "P5 *  *  *  *  *  *  *  *  * \n"
				 "P6 *  * +FU+FU *  * -GI *  * \n"
				 "P7+FU+FU *  * +GI * +KE * +FU\n"
				 "P8 * +OU+KI+GI * -TO *  *  * \n"
				 "P9+KY+KE *  *  *  *  *  * +KY\n"
				 "P+00KA00KA\n"
				 "P-00GI00KY00FU00FU00FU00FU00FU00FU\n"
				 "+\n"
                                   )); 
    Move bestMove;
    // +7162RYはpinned attackになっていない
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(BLACK,state,bestMove));
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY-KE *  * -KI+HI-KI * -OU\n"
				 "P2 *  *  *  *  *  * +UM-GI-HI\n"
				 "P3-FU * -FU+TO+KE *  * -FU * \n"
				 "P4 * -FU *  *  *  *  *  * -KY\n"
				 "P5 *  *  *  * +FU+OU+FU+KI-FU\n"
				 "P6 *  * +FU *  *  *  *  *  * \n"
				 "P7+FU+FU *  *  *  *  * +FU * \n"
				 "P8 *  *  *  * -UM *  *  *  * \n"
				 "P9+KY+KE *  *  *  *  *  *  * \n"
				 "P+00KI00GI00GI00KY00FU00FU00FU00FU00FU\n"
				 "P-00GI00KE00FU\n"
				 "+\n"
                                   )); 
    Move bestMove;
    // +0021KIはpinned attackになっていない
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(BLACK,state,bestMove));
  }
  {
    EffectState state(csa::read_board(
				 "P1+NK *  *  * +UM *  * -KE-KY\n"
				 "P2 *  * -GI *  *  * -KI+GI * \n"
				 "P3 *  *  *  *  *  * -KY *  * \n"
				 "P4 *  * -FU+FU * +KI-FU-OU-GI\n"
				 "P5 *  *  *  * -FU *  * -FU-FU\n"
				 "P6 * +FU+FU * +FU * +FU+KE * \n"
				 "P7 *  * +OU *  *  *  *  *  * \n"
				 "P8 *  *  *  * -KA *  *  *  * \n"
				 "P9+KY *  *  *  * +HI *  * +KY\n"
				 "P+00HI00KI00KE00FU00FU00FU00FU00FU\n"
				 "P-00KI00GI00FU00FU00FU\n"
				 "+\n"
                                   )); 
    Move bestMove;
    // +4434KI
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(BLACK,state,bestMove));
    TEST_CHECK(bestMove == Move(Square(4,4),Square(3,4),GOLD,PAWN,false,BLACK));
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY *  *  *  * -OU-GI * -KY\n"
				 "P2 *  *  * +NK *  *  *  *  * \n"
				 "P3 *  *  *  * -KI-FU+NK-FU * \n"
				 "P4-FU * -FU *  *  *  * -KI-FU\n"
				 "P5 *  *  * +KE-FU+FU-FU *  * \n"
				 "P6+FU-HI+FU *  *  *  * +FU+FU\n"
				 "P7 *  *  * -KA * +KI *  *  * \n"
				 "P8 *  *  *  *  *  * +GI+OU * \n"
				 "P9+KY *  *  *  * +KI *  * +KY\n"
				 "P+00KA00GI00FU\n"
				 "P-00HI00GI00KE00FU00FU00FU00FU00FU\n"
				 "+\n"
                                   )); 
    Move bestMove;
    // +6553KE
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(BLACK,state,bestMove));
    TEST_CHECK(bestMove == Move(Square(6,5),Square(5,3),KNIGHT,GOLD,false,BLACK));
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY *  *  *  * -OU-GI * -KY\n"
				 "P2 *  *  * +NK *  *  *  *  * \n"
				 "P3 *  *  *  * -KI-KI+NK-FU * \n"
				 "P4-FU * -FU *  *  *  * -KI-FU\n"
				 "P5 *  *  * +KE-FU+FU-FU *  * \n"
				 "P6+FU-HI+FU *  *  *  * +FU+FU\n"
				 "P7 *  *  * -KA * -FU *  *  * \n"
				 "P8 *  *  *  *  *  * +GI+OU * \n"
				 "P9+KY *  *  *  * +KI *  * +KY\n"
				 "P+00KA00GI00FU\n"
				 "P-00HI00GI00KE00FU00FU00FU00FU00FU\n"
				 "+\n"
                                   )); 
    Move bestMove;
    // +6553KE is captured by 4353KI
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(BLACK,state,bestMove));
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY *  *  *  * -OU-GI * -KY\n"
				 "P2 *  *  * +NK *  *  *  *  * \n"
				 "P3 *  *  *  * -KI-KI+NK-FU * \n"
				 "P4-FU * -FU *  * +HI * -KI-FU\n"
				 "P5 *  *  * +KE-FU+FU-FU *  * \n"
				 "P6+FU-HI+FU *  *  *  * +FU+FU\n"
				 "P7 *  *  * -KA * -FU *  *  * \n"
				 "P8 *  *  *  *  *  * +GI+OU * \n"
				 "P9+KY *  *  *  * +KI *  * +KY\n"
				 "P+00KA00GI00FU\n"
				 "P-00GI00KE00FU00FU00FU00FU00FU\n"
				 "+\n"
                                   )); 
    Move bestMove;
    // +6553KE is valid because 43KI is pinned
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(BLACK,state,bestMove));
    TEST_CHECK(bestMove == Move(Square(6,5),Square(5,3),KNIGHT,GOLD,false,BLACK));
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY-KE * -FU *  *  *  * -KY\n"
				 "P2 *  *  *  *  *  * -GI *  * \n"
				 "P3-FU-FU-FU *  *  * -GI * -FU\n"
				 "P4 *  *  *  * -OU *  *  *  * \n"
				 "P5 *  * +KE * -KA * +KI *  * \n"
				 "P6 *  *  * +RY+KE-RY-FU *  * \n"
				 "P7+FU+FU+OU *  *  * +FU * +FU\n"
				 "P8 *  * +KI *  * -TO *  * +KA\n"
				 "P9+KY *  *  *  *  *  * +KE+KY\n"
				 "P+00KI00GI00FU00FU00FU00FU\n"
				 "P-00KI00GI00FU00FU00FU\n"
				 "+\n"
                                   )); 
    Move bestMove;
    // +6663RYは自殺手
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(BLACK,state,bestMove));
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY *  * -KE *  *  *  * +RY\n"
				 "P2 *  * -OU *  *  *  *  *  * \n"
				 "P3 *  *  * -GI-KI * -KE-FU * \n"
				 "P4-FU-FU-FU-FU *  *  *  * -FU\n"
				 "P5-KE *  *  *  * -FU *  *  * \n"
				 "P6+GI * +FU+KE+FU * +FU *  * \n"
				 "P7+FU+FU+OU * +KI *  *  * +FU\n"
				 "P8 *  *  *  *  *  *  *  * -RY\n"
				 "P9+KY * -KA *  *  *  *  * +KY\n"
				 "P+00KI00GI00GI00KY00FU00FU00FU00FU\n"
				 "P-00KA00KI00FU\n"
				 "-\n"
                                   )); 
    Move bestMove;
    // -7968UMは88の利きを消す
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(WHITE,state,bestMove));
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY-KE+KA+KA *  * -OU-KE-KY\n"
				 "P2 *  *  *  *  * -GI-KI *  * \n"
				 "P3-FU * -FU-FU *  *  *  *  * \n"
				 "P4 * -HI *  *  *  *  * -FU-FU\n"
				 "P5 * -FU+FU+FU+KI+GI-FU *  * \n"
				 "P6 *  *  *  *  *  *  *  * +FU\n"
				 "P7+FU+FU *  *  * +FU-RY+FU+OU\n"
				 "P8 *  *  *  *  *  *  *  *  * \n"
				 "P9+KY *  *  *  *  *  * -NK+KY\n"
				 "P+00KI00GI00FU00FU\n"
				 "P-00KI00GI00KE00FU00FU\n"
				 "-\n"
                                   )); 
    Move bestMove;
    // -3728RYは26の利きを消す
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(WHITE,state,bestMove));
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY-KE *  *  *  *  *  * -KY\n"
				 "P2 *  *  *  *  * +GI * -OU * \n"
				 "P3-FU *  * -FU * -GI-KE *  * \n"
				 "P4 *  *  *  *  *  * -FU *  * \n"
				 "P5 * -FU+FU-RY * -FU *  * -FU\n"
				 "P6 *  *  *  *  *  * +KI+FU * \n"
				 "P7+FU+FU *  *  *  * +KE+GI+FU\n"
				 "P8+KY *  *  *  * +KI *  * +KY\n"
				 "P9 * +KE-HI *  *  * -KI * +OU\n"
				 "P+00KA00KI\n"
				 "P-00KA00GI00FU00FU00FU00FU00FU00FU00FU\n"
				 "-\n"
                                   )); 
    Move bestMove;
    // 
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(WHITE,state,bestMove));
    // open moveでも詰みだが，見つけられるのは寄る手
    TEST_CHECK(bestMove == Move(Square(3,9),Square(2,9),GOLD,Ptype_EMPTY,false,WHITE));
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY-KE * +RY *  *  *  * -KY\n"
				 "P2 *  *  *  *  *  * -GI *  * \n"
				 "P3-FU-FU-FU *  *  * -GI * -FU\n"
				 "P4 *  *  *  * -UM *  *  *  * \n"
				 "P5 *  *  *  * -OU * -FU *  * \n"
				 "P6 *  *  * -FU+KE-RY *  *  * \n"
				 "P7+FU+FU+OU *  *  * +FU * +FU\n"
				 "P8 *  * +KI+KI * -TO *  * +KA\n"
				 "P9+KY+KE *  *  *  *  * +KE+KY\n"
				 "P+00KI00GI00FU00FU00FU00FU\n"
				 "P-00KI00GI00FU00FU00FU\n"
				 "+\n"
                                   )); 
    Move bestMove;
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(BLACK,state,bestMove));
    // +7766OU などという王手をしてはいけない
    TEST_CHECK(bestMove != Move(Square(7,7),Square(6,6),KING,PAWN,false,BLACK));
  }
  {
    EffectState state(csa::read_board(
				 "P1+NK *  *  * +UM *  * -KE-KY\n"
				 "P2 *  * -GI *  *  * -KI+GI * \n"
				 "P3 *  *  *  *  *  * -KY * +HI\n"
				 "P4 *  * -FU+FU *  * -FU-OU-GI\n"
				 "P5 *  *  *  * -FU *  * -FU-FU\n"
				 "P6 * +FU+FU * +FU * +FU+KE * \n"
				 "P7 *  * +OU *  *  *  *  *  * \n"
				 "P8 *  *  *  * -KA *  *  *  * \n"
				 "P9+KY *  *  *  * +HI *  * +KY\n"
				 "P+00KI00KI00KE00FU00FU00FU00FU00FU\n"
				 "P-00KI00GI00FU00FU00FU\n"
				 "+\n"
                                   )); 
    // +2614KEは王手になっていない
    Move bestMove;
    // 
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(BLACK,state,bestMove));
  }
  {
    EffectState state(csa::read_board(
				 "P1+NK *  *  * +UM+RY * -KE-KY\n"
				 "P2 *  * -GI *  *  * -KI+GI-OU\n"
				 "P3 *  *  *  *  *  * -KY *  * \n"
				 "P4 *  * -FU+FU *  * -FU * -GI\n"
				 "P5 *  *  *  * -FU *  * -FU-FU\n"
				 "P6 * +FU+FU * +FU * +FU+KE * \n"
				 "P7 *  * +OU *  *  *  *  *  * \n"
				 "P8 *  *  *  * -KA *  *  *  * \n"
				 "P9+KY *  *  *  *  *  *  * +KY\n"
				 "P+00HI00KI00KI00KE00FU00FU00FU00FU00FU\n"
				 "P-00KI00GI00FU00FU00FU\n"
				 "+\n"
                                   )); 
    // +4121RY は22に駒が無くて23に利きが通っていれば詰み
    Move bestMove;
    // 
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(BLACK,state,bestMove));
  }
  {
    EffectState state(csa::read_board(
				 "P1+NK *  *  * +UM *  * -KE-KY\n"
				 "P2 *  * -GI *  *  * -KI *  * \n"
				 "P3 *  *  *  *  *  * -KY * +NG\n"
				 "P4 *  * -FU+FU *  * -FU-OU-GI\n"
				 "P5 *  *  *  * -FU *  * -FU-FU\n"
				 "P6 * +FU+FU * +FU * +FU+KE * \n"
				 "P7 *  * +OU *  *  *  *  *  * \n"
				 "P8 *  *  *  * -KA *  *  *  * \n"
				 "P9+KY *  *  *  * +HI *  * +KY\n"
				 "P+00HI00KI00KI00KE00FU00FU00FU00FU00FU\n"
				 "P-00KI00GI00FU00FU00FU\n"
				 "+\n"
                                   )); 
    // +1314NG で元々14に利きがなかったので，移動後も利きがないものと思ってしまっている．
    Move bestMove;
    // 
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(BLACK,state,bestMove));
  }

  {
    EffectState state(csa::read_board(
				 "P1 *  *  * -KI *  *  *  * -KY\n"
				 "P2 * +KI-GI *  *  *  *  *  * \n"
				 "P3+GI-FU-FU *  *  *  *  * -FU\n"
				 "P4-FU * -KE *  * -FU-FU-FU * \n"
				 "P5 *  * +KA *  *  *  *  *  * \n"
				 "P6 *  *  *  * -FU *  * +HI * \n"
				 "P7 * +FU+KE+FU * +KA-OU * +FU\n"
				 "P8 *  * +OU+GI *  *  *  *  * \n"
				 "P9 *  * +KI * -NG+KY *  * +KY\n"
				 "P+00KE00FU\n"
				 "P-00HI00KI00KE00KY00FU00FU00FU00FU00FU00FU\n"
				 "+\n"
                                   )); 
    // 
    Move bestMove;
    // +7548KA で47の利きをBLOCKすることを忘れていると詰みだと思ってしまう．
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(BLACK,state,bestMove));
  }

  {
    EffectState state(csa::read_board(
				 "P1-KY *  *  *  *  *  *  * -KY\n"
				 "P2-OU+GI-GI * -KI *  *  *  * \n"
				 "P3-KE-FU *  *  *  * +TO * -FU\n"
				 "P4+FU * -FU-GI *  *  *  *  * \n"
				 "P5 *  *  *  *  *  * -KA *  * \n"
				 "P6 *  * +FU+FU-FU *  *  *  * \n"
				 "P7+KE+FU+KA+KI *  *  *  * +FU\n"
				 "P8 *  * +OU *  * +KI *  *  * \n"
				 "P9+KY *  *  * +FU *  * +KE+KY\n"
				 "P+00HI00GI00KE00FU00FU00FU00FU00FU\n"
				 "P-00HI00KI00FU00FU\n"
				 "+\n"
                                   )); 
    // 
    Move bestMove;
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(BLACK,state,bestMove));
    TEST_CHECK(bestMove == Move(Square(9,4),Square(9,3),PPAWN,KNIGHT,true,BLACK));
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY+UM+RY+HI *  * -KI * -KY\n"
				 "P2 *  *  *  *  *  * -KI *  * \n"
				 "P3 *  *  * -FU * -KI-KE * -OU\n"
				 "P4-FU *  *  *  *  * +FU+KY-FU\n"
				 "P5 * -FU * +FU * -FU *  *  * \n"
				 "P6+FU *  *  * +FU-KI * +FU+FU\n"
				 "P7 * +FU *  *  * +FU * +OU * \n"
				 "P8 *  * +FU *  *  *  *  *  * \n"
				 "P9 *  *  *  *  * -NG-GI+KE-UM\n"
				 "P+00GI00GI00KE00KY00FU00FU00FU00FU\n"
				 "P-00KE\n"
				 "-\n"
                                   )); 
    // 
    Move bestMove;
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(WHITE,state,bestMove));
    TEST_CHECK_EQUAL(Move(Square(1,9),Square(2,8),PBISHOP,Ptype_EMPTY,false,WHITE),bestMove);
  }
  {
    EffectState state(csa::read_board(
				 "P1 * +TO *  * -HI *  * -KE-KY\n"
				 "P2 *  *  *  *  *  * -KI *  * \n"
				 "P3 *  *  * -GI * -KI * -FU * \n"
				 "P4-OU+KY *  * -FU-GI *  * -FU\n"
				 "P5-KA * +FU *  * -FU-FU+KE * \n"
				 "P6 *  * -FU * +FU *  * +HI+FU\n"
				 "P7-TO *  * +FU+KA *  *  *  * \n"
				 "P8 * -GI+OU * +KI+GI *  *  * \n"
				 "P9 * -NY * +KI *  *  *  *  * \n"
				 "P+00FU00FU00FU00FU\n"
				 "P-00KE00KE00KY00FU00FU\n"
				 "-\n"
                                   )); 
    // 
    Move bestMove;
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(WHITE,state,bestMove));
    TEST_CHECK(bestMove == Move(Square(7,6),Square(7,7),PPAWN,Ptype_EMPTY,true,WHITE) ||
		   bestMove == Move(Square(9,5),Square(7,7),PBISHOP,Ptype_EMPTY,true,WHITE));
  }
  {
    EffectState state(csa::read_board(
				 "P1 *  *  *  * -KI *  *  * -KY\n"
				 "P2 * +NG+TO *  * -KI-OU *  * \n"
				 "P3 *  *  * -FU-FU-FU-KE+FU-FU\n"
				 "P4-FU *  *  * -KE * -KY-GI * \n"
				 "P5 *  *  * +UM *  *  *  *  * \n"
				 "P6+FU *  *  * +HI *  *  *  * \n"
				 "P7 * +FU * +FU+FU+FU+FU * +FU\n"
				 "P8 *  *  *  * +HI+OU * -KA * \n"
				 "P9+KY+KE-TO *  * +KI * +GI+KY\n"
				 "P-00KI00GI00KE00FU00FU00FU\n"
				 "-\n"
                                   )); 
    // 
    Move bestMove;
    // -2837UM で39の利きが消えることを忘れていると詰みだと思ってしまう．
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(WHITE,state,bestMove));
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY-KE * +RY *  *  *  * -KY\n"
				 "P2 *  *  *  *  *  * -GI *  * \n"
				 "P3-FU-FU-FU *  *  * -GI * -FU\n"
				 "P4 *  *  * +KI *  *  *  *  * \n"
				 "P5 *  *  *  * -OU * -FU *  * \n"
				 "P6 *  *  * -FU-UM-RY *  *  * \n"
				 "P7+FU+FU+OU *  *  * +FU * +FU\n"
				 "P8 *  * +KI+KI * -TO *  * +KA\n"
				 "P9+KY+KE *  *  *  *  * +KE+KY\n"
				 "P+00GI00KE00FU00FU00FU00FU\n"
				 "P-00KI00GI00FU00FU00FU\n"
				 "+\n"
                                   )); 
    Move bestMove;
    // +6454KI
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(BLACK,state,bestMove));
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY *  *  *  *  * -KA * -KY\n"
				 "P2 *  * -KI *  * -KE+NG *  * \n"
				 "P3 *  *  * -FU-OU-FU-KE-FU * \n"
				 "P4-FU * -FU *  *  *  *  * -FU\n"
				 "P5 *  * +FU * -FU * -FU *  * \n"
				 "P6 *  *  * +HI * +KE *  *  * \n"
				 "P7+FU+FU * +FU+GI+FU+FU * +FU\n"
				 "P8 *  * -NG * +KI *  *  *  * \n"
				 "P9+KY *  *  * +OU * +KI-RY+KY\n"
				 "P+00FU\n"
				 "P-00KA00KI00GI00KE00FU00FU\n"
				 "-\n"
                                   )); 
    Move bestMove;
    // +2939RYは合駒なし
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(BLACK,state,bestMove));
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY-KE * +GI *  *  *  * -KY\n"
				 "P2 *  * +RY *  *  *  *  *  * \n"
				 "P3-OU-FU-FU * -KI *  *  *  * \n"
				 "P4-FU *  * -FU *  *  * -KA-FU\n"
				 "P5 * +FU *  * +FU+UM+KI-FU * \n"
				 "P6+FU * -RY * -GI+OU-FU *  * \n"
				 "P7 *  *  *  *  *  * -NG * +FU\n"
				 "P8 *  *  *  * +GI * +FU * +KY\n"
				 "P9+KY *  *  *  *  *  *  *  * \n"
				 "P+00KI00KE00FU\n"
				 "P-00KI00KE00KE00FU00FU00FU00FU00FU\n"
				 "-\n"
                                   )); 
    Move bestMove;
    // 05647NGはopen attack
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(WHITE,state,bestMove));
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY *  * -KI+TO *  *  *  * \n"
				 "P2 *  *  *  *  * -FU *  *  * \n"
				 "P3 *  *  *  *  *  * +RY * -KY\n"
				 "P4 *  * -OU-FU * +FU *  * -FU\n"
				 "P5-FU * -FU *  *  *  *  *  * \n"
				 "P6 *  *  * +FU *  *  *  *  * \n"
				 "P7+FU-TO * +KI *  * +FU * +FU\n"
				 "P8 * +GI+KI *  *  *  *  *  * \n"
				 "P9+KY+OU+GI-GI *  *  * -RY+KY\n"
				 "P+00KI00KE00KE00FU\n"
				 "P-00KA00KA00GI00KE00KE00FU00FU00FU00FU00FU\n"
				 "-\n"
                                   )); 
    Move bestMove;
    // -6978GIはpinnedだが初期状態ではdetectできない
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(WHITE,state,bestMove));
  }
  {
    EffectState state(csa::read_board(
      "P1 *  *  * -KI *  *  *  * -KY\n"
      "P2 * +KI-GI *  *  *  *  *  * \n"
      "P3+GI-FU-FU *  *  *  *  * -FU\n"
      "P4-FU * -KE *  * -FU-FU-FU * \n"
      "P5 *  * +KA *  *  *  * +HI * \n"
      "P6 *  *  *  * -FU-KY *  *  * \n"
      "P7 * +FU+KE+FU * +KA-OU * +FU\n"
      "P8 *  * +OU+GI-NG *  *  *  * \n"
      "P9 *  * +KI *  * +KY *  * +KY\n"
      "P+00KE00FU\n"
      "P-00HI00KI00KE00FU00FU00FU00FU00FU00FU\n"
      "+\n"
      ));
    // 
    Move bestMove;
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(BLACK,state,bestMove));
    TEST_CHECK(bestMove==Move(Square(2,9),KNIGHT,BLACK));
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY-KE * +GI *  *  *  * -KY\n"
				 "P2 *  * +RY *  *  *  *  *  * \n"
				 "P3-OU * -FU *  *  *  *  *  * \n"
				 "P4-FU-KE * -FU *  *  * -KA-FU\n"
				 "P5 *  *  *  * +FU * +KI-FU * \n"
				 "P6+FU * +KE * -GI+OU *  *  * \n"
				 "P7 * +HI * +GI *  * -TO * +FU\n"
				 "P8 *  *  *  *  * -GI+FU * +KY\n"
				 "P9+KY *  *  *  *  *  *  *  * \n"
				 "P+00KA00KE00FU00FU00FU\n"
				 "P-00KI00KI00KI00FU00FU00FU00FU00FU\n"
				 "+\n"
			 ));
    // 
    Move bestMove;
    TEST_CHECK(isImmediateCheck<true>(state,Move(Square(8,5),KNIGHT,BLACK)));
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(BLACK,state,bestMove));
    TEST_CHECK(bestMove==Move(Square(8,5),KNIGHT,BLACK));
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY-KE * +GI *  *  *  * -KY\n"
				 "P2 *  * +RY *  *  *  *  *  * \n"
				 "P3-OU * -FU * -KI *  *  *  * \n"
				 "P4-FU-FU * -FU *  *  * -KA-FU\n"
				 "P5 *  * +KA * +FU * +KI-FU * \n"
				 "P6+FU+FU *  * -GI+OU *  *  * \n"
				 "P7 *  * -RY+GI *  * -TO * +FU\n"
				 "P8 *  *  *  *  * -GI+FU * +KY\n"
				 "P9+KY *  *  *  *  *  *  *  * \n"
				 "P+00KE00FU\n"
				 "P-00KI00KI00KE00KE00FU00FU00FU00FU00FU\n"
				 "+\n"
			 ));
    // 
    Move bestMove;
    TEST_CHECK(isImmediateCheck<true>(state,Move(Square(8,5),KNIGHT,BLACK)));
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(BLACK,state,bestMove));
    TEST_CHECK(bestMove==Move(Square(8,5),KNIGHT,BLACK));
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY-KE * +GI *  *  *  * -KY\n"
				 "P2 *  * +RY *  *  *  *  *  * \n"
				 "P3-OU-FU-FU * -KI *  *  *  * \n"
				 "P4-FU *  * -FU *  * +UM-KA-FU\n"
				 "P5 * +FU *  * +FU * +KI-FU * \n"
				 "P6+FU * -RY * -GI+OU *  *  * \n"
				 "P7 *  *  * +GI *  * -TO * +FU\n"
				 "P8 *  *  *  *  * -GI+FU * +KY\n"
				 "P9+KY *  *  *  *  *  *  *  * \n"
				 "P+00KI00KE00FU\n"
				 "P-00KI00KE00KE00FU00FU00FU00FU00FU\n"
				 "-\n"
			 ));
    // 
    Move bestMove;
    TEST_CHECK(isImmediateCheck<true>(state,Move(Square(3,6),GOLD,WHITE)));
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(WHITE,state,bestMove));
  }
  {
    EffectState state(csa::read_board(
				 "P1 * +TO *  * -HI *  * -KE-KY\n"
				 "P2 *  *  *  *  *  * -KI *  * \n"
				 "P3 *  *  * -GI * -KI * -FU * \n"
				 "P4-OU+KY *  * -FU-GI *  * -FU\n"
				 "P5-KA * -FU *  * -FU-FU+KE * \n"
				 "P6 *  * +FU+KA+FU *  * +HI+FU\n"
				 "P7-TO *  * +FU *  *  *  *  * \n"
				 "P8 * -GI+OU * +KI+GI *  *  * \n"
				 "P9 * -NY * +KI *  *  *  *  * \n"
				 "P+00FU00FU00FU00FU\n"
				 "P-00KE00KE00KY00FU00FU\n"
				 "-\n"
                                   ));
    // 
    Move bestMove;
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(WHITE,state,bestMove) ||
		   (std::cerr << bestMove << std::endl,0)
		   );
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY *  *  *  *  *  *  * -KY\n"
				 "P2 * -HI *  * +KI * -OU *  * \n"
				 "P3-FU * -FU-FU *  *  * -FU * \n"
				 "P4 *  *  *  *  * +UM-FU *  * \n"
				 "P5 *  *  *  *  *  *  *  * -FU\n"
				 "P6+FU * +FU *  * +KE+FU *  * \n"
				 "P7 * +FU *  *  * +FU+KE+FU * \n"
				 "P8 *  * +KI *  *  * +GI+OU * \n"
				 "P9+KY+KE *  *  * +KI *  * +KY\n"
				 "P+00KI00FU00FU\n"
				 "P-00HI00KA00GI00GI00GI00KE00FU00FU00FU00FU\n"
				 "+\n"
                                   ));
    // 22KI
    Move bestMove;
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(BLACK,state,bestMove));
    TEST_CHECK_EQUAL(Move(Square(2,2),GOLD,BLACK),bestMove);
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY-KE *  *  *  *  *  * -KY\n"
				 "P2 *  * -KI *  *  *  *  *  * \n"
				 "P3 * -FU-GI * -FU-FU-OU *  * \n"
				 "P4-FU *  * -FU *  * -FU-FU * \n"
				 "P5 *  *  *  *  * +GI *  * -FU\n"
				 "P6+FU * +KI+FU+KA *  * -UM * \n"
				 "P7 *  * +KE * +FU+FU+FU * +FU\n"
				 "P8 * -RY+GI *  * -HI *  *  * \n"
				 "P9+KY *  * +KI+OU *  *  * +KY\n"
				 "P-00KI00GI00KE00KE00FU00FU00FU00FU\n"
				 "-\n"
                                   ));
    // -0049KI!!
    Move bestMove;
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(WHITE,state,bestMove));
    TEST_CHECK_EQUAL(Move(Square(4,9),GOLD,WHITE),bestMove);
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY-GI * +TO *  *  *  *  * \n"
				 "P2-FU-OU-KI * -GI *  *  *  * \n"
				 "P3 *  *  *  *  *  *  *  * -FU\n"
				 "P4+FU-FU * +GI *  *  *  *  * \n"
				 "P5 *  * -FU+KA *  * +FU *  * \n"
				 "P6+OU *  *  *  * +FU *  *  * \n"
				 "P7 * +FU *  *  * -RY-FU *  * \n"
				 "P8 * +UM *  * -TO *  *  *  * \n"
				 "P9+KY *  * +KI+KY-RY *  *  * \n"
				 "P+00KI00KI00GI00KE00KY00FU00FU00FU00FU00FU\n"
				 "P-00KE00KE00KE00FU00FU\n"
				 "+\n"
                                   ));
    // 74KEは角をふさぐ
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(BLACK,state));
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY+RY *  *  *  *  * -KE-KY\n"
				 "P2 *  * +TO *  * -KI-OU-KE * \n"
				 "P3 *  *  *  *  * -KI-GI-FU * \n"
				 "P4 *  * +GI * -FU-FU *  * -FU\n"
				 "P5-FU *  * +FU *  *  *  *  * \n"
				 "P6 *  *  *  * +FU+FU *  * +FU\n"
				 "P7+FU-TO *  *  * +KI+GI+FU * \n"
				 "P8 *  *  * -UM-TO+KI+OU *  * \n"
				 "P9+KY-UM *  *  *  *  * +KE+KY\n"
				 "P+00HI00KE00FU00FU00FU\n"
				 "P-00GI00FU\n"
				 "+\n"
                                   ));
    // 31HI
    Move bestMove;
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(BLACK,state,bestMove));
    TEST_CHECK_EQUAL(Move(Square(3,1),ROOK,BLACK),bestMove);
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY+HI *  *  * -KI * -KE * \n"
				 "P2 *  *  *  *  * -GI-OU *  * \n"
				 "P3-FU *  * +TO-FU-GI * -FU * \n"
				 "P4 *  *  *  *  * -FU-FU *  * \n"
				 "P5 *  * -TO * +KI *  *  *  * \n"
				 "P6 *  *  *  *  *  * -RY *  * \n"
				 "P7 * +FU * -KY * +FU+FU-NK * \n"
				 "P8 *  *  *  * +FU+GI-KI *  * \n"
				 "P9 *  *  * +KA+OU * +KI *  * \n"
				 "P+00GI00KY00FU00FU00FU00FU00FU\n"
				 "P-00KA00KE00KE00KY00FU00FU\n"
				 "-\n"
                                   ));
    // 68KA
    Move bestMove;
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(WHITE,state,bestMove));
    TEST_CHECK_EQUAL(Move(Square(6,8),BISHOP,WHITE),bestMove);
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY+NG *  *  *  *  *  * +UM\n"
				 "P2 *  *  *  *  *  *  *  * -KY\n"
				 "P3 * -OU-FU *  *  *  *  *  * \n"
				 "P4-FU * -GI * -KI *  * -FU-FU\n"
				 "P5 * +KE * -FU *  *  * +KE * \n"
				 "P6+FU+OU *  * +FU+FU *  * +FU\n"
				 "P7 *  *  *  * +GI *  *  *  * \n"
				 "P8 *  *  *  *  *  *  * +HI * \n"
				 "P9+KY *  * -UM *  *  *  * +KY\n"
				 "P+00HI00KI00KI00GI00KE00FU00FU00FU00FU00FU00FU00FU\n"
				 "P-00KI00KE00FU00FU\n"
				 "+\n"
                                   ));
    // 82HI
    Move bestMove;
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(BLACK,state,bestMove));
    TEST_CHECK_EQUAL(Move(Square(8,2),ROOK,BLACK),bestMove);
  }
  {
    EffectState state(csa::read_board(
				 "P1-KY+UM+RY+HI *  * -KI * -KY\n"
				 "P2 *  *  *  *  *  * -KI *  * \n"
				 "P3 *  *  * -FU * -KI-KE * -OU\n"
				 "P4-FU *  *  *  *  * +FU+KY-FU\n"
				 "P5 * -FU * +FU * -FU *  *  * \n"
				 "P6+FU *  *  * +FU *  * +FU+FU\n"
				 "P7 * +FU *  *  * +FU+OU *  * \n"
				 "P8 *  * +FU *  *  *  *  * -UM\n"
				 "P9 *  *  *  *  * -NG-GI+KE * \n"
				 "P+00GI00GI00KE00KY00FU00FU00FU00FU\n"
				 "P-00KI00KE\n"
				 "-\n"
                                   ));
    // 27金は46への馬の利きをふさぐ
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(WHITE,state));
  }

  {
    EffectState state(csa::read_board(
				 "P1-KY+HI *  *  * -KI * -KE * \n"
				 "P2 *  * +TO *  * -GI-OU *  * \n"
				 "P3-FU *  *  * -FU-GI * -FU * \n"
				 "P4 *  *  *  *  * -FU-FU *  * \n"
				 "P5 *  * -TO * +KI *  *  *  * \n"
				 "P6 *  *  * -RY *  *  *  *  * \n"
				 "P7 * +FU *  *  * +FU+FU-NK * \n"
				 "P8 *  *  *  * +FU+GI-KI *  * \n"
				 "P9 *  *  *  * +OU * +KI *  * \n"
				 "P+00KA00GI00KY00FU00FU00FU00FU00FU\n"
				 "P-00KA00KE00KE00KY00KY00FU00FU\n"
				 "-\n"
                                   ));
    // 67桂は68,69への龍の利きをふさぐ
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(WHITE,state));
  }
  {
    EffectState state(csa::read_board(
				 "P1 * +NY *  *  *  *  * -KE-KY\n"
				 "P2 *  *  *  * -FU-KI-OU *  * \n"
				 "P3 * +HI *  *  *  *  * -FU-FU\n"
				 "P4 *  *  * -FU+FU-FU-FU *  * \n"
				 "P5+HI+FU+FU * -KE *  *  *  * \n"
				 "P6 *  * -KY+GI *  *  *  *  * \n"
				 "P7-GI+OU *  *  * +FU+FU+FU+FU\n"
				 "P8 *  *  *  *  *  *  *  *  * \n"
				 "P9 *  *  *  *  *  *  * +KE+KY\n"
				 "P+00KA00KA00KI00KI00GI00GI00KE00FU00FU00FU00FU00FU\n"
				 "P-00KI\n"
				 "-\n"
                                   ));
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(WHITE,state));
  }
  {
    EffectState state(csa::read_board(
				 "P1 *  *  *  *  *  *  * -OU * \n"
				 "P2 *  *  *  *  *  *  *  *  * \n"
				 "P3 *  *  *  *  *  *  * +TO * \n"
				 "P4 *  *  *  *  *  *  *  *  * \n"
				 "P5 *  *  *  *  *  *  *  *  * \n"
				 "P6 *  *  *  *  *  *  *  *  * \n"
				 "P7 *  *  *  *  *  *  *  *  * \n"
				 "P8 *  *  *  *  *  *  *  *  * \n"
				 "P9 *  * +OU *  *  *  *  *  * \n"
				 "P+00KI\n"
				 "P-00AL\n"
				 "+\n"));
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(BLACK,state));
  }
  {
    EffectState state(csa::read_board(
				 "P1 *  *  *  *  *  * -FU-OU-FU\n"
				 "P2 *  *  *  *  *  *  *  *  * \n"
				 "P3 *  *  *  *  *  *  * +TO-KE\n"
				 "P4 *  *  *  *  *  *  *  *  * \n"
				 "P5 *  *  *  *  *  *  *  *  * \n"
				 "P6 *  *  *  *  *  *  *  *  * \n"
				 "P7 *  *  *  *  *  *  *  *  * \n"
				 "P8 *  *  *  *  *  *  *  *  * \n"
				 "P9 *  * +OU *  *  *  *  *  * \n"
				 "P+00KE\n"
				 "P-00AL\n"
				 "+\n"));
    TEST_CHECK(ImmediateCheckmate::hasCheckmateMove(BLACK,state));
  }
  {
    EffectState state(csa::read_board(
				 "P1 *  *  *  *  *  *  *  *  * \n"
				 "P2 *  *  *  *  *  *  * -OU * \n"
				 "P3 *  *  *  *  *  *  *  *  * \n"
				 "P4 *  *  *  *  *  *  * +TO * \n"
				 "P5 *  *  *  *  *  *  *  *  * \n"
				 "P6 *  *  *  *  *  *  *  *  * \n"
				 "P7 *  *  *  *  *  *  *  *  * \n"
				 "P8 *  *  *  *  *  *  *  *  * \n"
				 "P9 *  * +OU *  *  *  *  *  * \n"
				 "P+00KI\n"
				 "P-00AL\n"
				 "+\n"));
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(BLACK,state));
  }
  {
    EffectState state(csa::read_board(
				 "P1 *  *  *  *  *  *  * -OU-FU\n"
				 "P2 *  *  *  *  *  *  *  *  * \n"
				 "P3 *  *  *  *  *  *  * +TO-KE\n"
				 "P4 *  *  *  *  *  *  *  *  * \n"
				 "P5 *  *  *  *  *  *  *  *  * \n"
				 "P6 *  *  *  *  *  *  *  *  * \n"
				 "P7 *  *  *  *  *  *  *  *  * \n"
				 "P8 *  *  *  *  *  *  *  *  * \n"
				 "P9 *  * +OU *  *  *  *  *  * \n"
				 "P+00KE\n"
				 "P-00AL\n"
				 "+\n"));
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(BLACK,state));
  }
  {
    EffectState state(csa::read_board(
				 "P1 *  *  * +UM *  *  *  *  * \n"
				 "P2+GI *  * -KE-KY *  *  * -UM\n"
				 "P3-FU+TO * -OU-FU+GI-KE *  * \n"
				 "P4 *  * -FU *  *  *  * -FU * \n"
				 "P5 *  *  *  * +KY *  * -RY * \n"
				 "P6 * -HI * -FU *  *  *  *  * \n"
				 "P7+FU * +FU-KI * +FU *  * -NK\n"
				 "P8 *  *  *  *  * +GI *  *  * \n"
				 "P9+KY+KE+KI+OU+KI *  *  *  * \n"
				 "P+00KI00GI00KY00FU00FU00FU00FU00FU00FU\n"
				 "P-00FU00FU00FU\n"
				 "+\n"
                                   ));
    // 65KYは離れたところからのdropなのでまだ打てない．
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(BLACK,state));
  }
  {
    EffectState state(csa::read_board(
				 "P1+RY-KY *  * -OU * -FU+UM-KY\n"
				 "P2 * -KI *  * -KI * -KI *  * \n"
				 "P3 * +KE-FU-KA-FU-FU *  *  * \n"
				 "P4+OU *  *  * +KE *  * -FU-FU\n"
				 "P5-FU * +FU-GI *  *  *  *  * \n"
				 "P6 * +GI * -FU * +GI * +KI+FU\n"
				 "P7+FU *  * +FU+FU+FU *  *  * \n"
				 "P8 *  * -NK *  *  *  *  *  * \n"
				 "P9+KY+KE *  *  *  * +FU * +KY\n"
				 "P+00HI00GI\n"
				 "P-00FU00FU00FU\n"
				 "+\n"
                                   ));
    // +0071HIは離れたところからのdropなので打てない．
    TEST_CHECK(!ImmediateCheckmate::hasCheckmateMove(BLACK,state));
  }
  {
    // no checkmate
    EffectState state
      (csa::read_board("P1-KI *  *  * +TO+TO+TO+TO+NY\n"
		 "P2 * -FU+NK *  *  *  *  *  * \n"
		 "P3+RY-TO * -OU-FU-FU * -FU-KY\n"
		 "P4 *  *  * -GI-KI-TO *  * +RY\n"
		 "P5-UM-NK+KY+TO * -TO *  *  * \n"
		 "P6 * +FU *  *  * -GI-GI+FU * \n"
		 "P7 *  * +FU * +TO-GI *  * +KE\n"
		 "P8 *  *  *  * +TO * +FU * +KY\n"
		 "P9+UM *  *  *  *  * +KI * -KI\n"
		 "P-00KE\n"
		 "+\n"
	));
    Move check_move;
    const bool is_checkmate
      = ImmediateCheckmate::hasCheckmateMove(BLACK,state, check_move);
    // +6574TOは -5372OUで後1000手くらい詰まない
    TEST_CHECK(check_move != Move(Square(6,5),Square(7,4),PPAWN,Ptype_EMPTY,false,BLACK));
    TEST_CHECK(! is_checkmate);
  }
}

void test_ki2() {
  {
    EffectState state;
    Move move(Square(7,7), Square(7,6), PAWN, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗７六歩", to_ki2(move, state));
  }
  {
    auto state(csa::read_board("P1 *  *  *  * +TO *  * -OU * \n"
                               "P2 *  * +KI *  *  *  *  *  * \n"
                               "P3+KI *  * +TO *  *  * +OU * \n"
                               "P4 *  *  *  * +TO *  *  *  * \n"
                               "P5 *  *  * +GI *  *  *  *  * \n"
                               "P6 *  *  *  *  *  *  *  *  * \n"
                               "P7 *  * +GI *  * +GI *  *  * \n"
                               "P8 *  *  *  *  *  *  *  *  * \n"
                               "P9 *  *  *  *  *  *  *  *  * \n"
                               "P+00AL\n"
                               "-\n"));
    const Move m82u(Square(9,3), Square(8,2), GOLD, Ptype_EMPTY, false, BLACK);
    const Move m82y(Square(7,2), Square(8,2), GOLD, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗８二金上", to_ki2(m82u, state));
    TEST_CHECK_EQUAL(u8"☗８二金寄", to_ki2(m82y, state));
    const Move m52h(Square(5,1), Square(5,2), PPAWN, Ptype_EMPTY, false, BLACK);
    const Move m52u(Square(6,3), Square(5,2), PPAWN, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗５二と引", to_ki2(m52h, state));
    TEST_CHECK_EQUAL(u8"☗５二と上", to_ki2(m52u, state));
    const Move m53y(Square(6,3), Square(5,3), PPAWN, Ptype_EMPTY, false, BLACK);
    const Move m53u(Square(5,4), Square(5,3), PPAWN, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗５三と寄", to_ki2(m53y, state));
    TEST_CHECK_EQUAL(u8"☗５三と上", to_ki2(m53u, state));
    const Move m76h(Square(6,5), Square(7,6), SILVER, Ptype_EMPTY, false, BLACK);
    const Move m76u(Square(7,7), Square(7,6), SILVER, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗７六銀引", to_ki2(m76h, state));
    TEST_CHECK_EQUAL(u8"☗７六銀上", to_ki2(m76u, state));
    const Move m56h(Square(6,5), Square(5,6), SILVER, Ptype_EMPTY, false, BLACK);
    const Move m56u(Square(4,7), Square(5,6), SILVER, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗５六銀引", to_ki2(m56h, state));
    TEST_CHECK_EQUAL(u8"☗５六銀上", to_ki2(m56u, state));
  }
  {
    auto state(csa::read_board("P1 *  *  *  *  *  *  * -OU * \n"
                               "P2+KI * +KI *  *  *  *  *  * \n"
                               "P3 *  *  *  *  *  *  * +OU * \n"
                               "P4 *  *  *  *  *  *  *  *  * \n"
                               "P5 *  *  * +GI * +GI *  *  * \n"
                               "P6 *  *  *  *  *  *  *  *  * \n"
                               "P7 *  *  *  *  *  *  *  *  * \n"
                               "P8 *  *  *  *  *  *  *  *  * \n"
                               "P9 * +KI+KI *  *  * +GI+GI * \n"
                               "P+00AL\n"
                               "-\n"));
    const Move m81l(Square(9,2), Square(8,1), GOLD, Ptype_EMPTY, false, BLACK);
    const Move m81r(Square(7,2), Square(8,1), GOLD, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗８一金左", to_ki2(m81l, state));
    TEST_CHECK_EQUAL(u8"☗８一金右", to_ki2(m81r, state));
    const Move m82l(Square(9,2), Square(8,2), GOLD, Ptype_EMPTY, false, BLACK);
    const Move m82r(Square(7,2), Square(8,2), GOLD, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗８二金左", to_ki2(m82l, state));
    TEST_CHECK_EQUAL(u8"☗８二金右", to_ki2(m82r, state));
    const Move m56l(Square(6,5), Square(5,6), SILVER, Ptype_EMPTY, false, BLACK);
    const Move m56r(Square(4,5), Square(5,6), SILVER, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗５六銀左", to_ki2(m56l, state));
    TEST_CHECK_EQUAL(u8"☗５六銀右", to_ki2(m56r, state));
    const Move m78l(Square(8,9), Square(7,8), GOLD, Ptype_EMPTY, false, BLACK);
    const Move m78s(Square(7,9), Square(7,8), GOLD, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗７八金左", to_ki2(m78l, state));
    TEST_CHECK_EQUAL(u8"☗７八金直", to_ki2(m78s, state));
    const Move m38s(Square(3,9), Square(3,8), SILVER, Ptype_EMPTY, false, BLACK);
    const Move m38r(Square(2,9), Square(3,8), SILVER, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗３八銀直", to_ki2(m38s, state));
    TEST_CHECK_EQUAL(u8"☗３八銀右", to_ki2(m38r, state));
  }
  {
    auto state(csa::read_board("P1 *  *  *  *  *  *  * -OU * \n"
                               "P2 *  *  *  *  *  *  *  *  * \n"
                               "P3 *  *  * +KI+KI+KI * +OU * \n"
                               "P4 *  *  *  *  *  *  *  *  * \n"
                               "P5 *  *  *  *  *  *  *  *  * \n"
                               "P6 *  *  *  *  *  *  *  *  * \n"
                               "P7 * +TO *  *  *  * +GI * +GI\n"
                               "P8+TO *  *  *  *  *  *  *  * \n"
                               "P9+TO+TO+TO *  *  * +GI+GI * \n"
                               "P+00AL\n"
                               "-\n"));
    const Move m52l(Square(6,3), Square(5,2), GOLD, Ptype_EMPTY, false, BLACK);
    const Move m52s(Square(5,3), Square(5,2), GOLD, Ptype_EMPTY, false, BLACK);
    const Move m52r(Square(4,3), Square(5,2), GOLD, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗５二金左", to_ki2(m52l, state));
    TEST_CHECK_EQUAL(u8"☗５二金直", to_ki2(m52s, state));
    TEST_CHECK_EQUAL(u8"☗５二金右", to_ki2(m52r, state));
    const Move m88r(Square(7,9), Square(8,8), PPAWN, Ptype_EMPTY, false, BLACK);
    const Move m88s(Square(8,9), Square(8,8), PPAWN, Ptype_EMPTY, false, BLACK);
    const Move m88y(Square(9,8), Square(8,8), PPAWN, Ptype_EMPTY, false, BLACK);
    const Move m88h(Square(8,7), Square(8,8), PPAWN, Ptype_EMPTY, false, BLACK);
    const Move m88lu(Square(9,9), Square(8,8), PPAWN, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗８八と右", to_ki2(m88r, state));
    TEST_CHECK_EQUAL(u8"☗８八と直", to_ki2(m88s, state));
    TEST_CHECK_EQUAL(u8"☗８八と寄", to_ki2(m88y, state));
    TEST_CHECK_EQUAL(u8"☗８八と引", to_ki2(m88h, state));
    TEST_CHECK_EQUAL(u8"☗８八と左上", to_ki2(m88lu, state));
    const Move m28s(Square(2,9), Square(2,8), SILVER, Ptype_EMPTY, false, BLACK);
    const Move m28r(Square(1,7), Square(2,8), SILVER, Ptype_EMPTY, false, BLACK);
    const Move m28lu(Square(3,9), Square(2,8), SILVER, Ptype_EMPTY, false, BLACK);
    const Move m28lh(Square(3,7), Square(2,8), SILVER, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗２八銀直", to_ki2(m28s, state));
    TEST_CHECK_EQUAL(u8"☗２八銀右", to_ki2(m28r, state));
    TEST_CHECK_EQUAL(u8"☗２八銀左上", to_ki2(m28lu, state));
    TEST_CHECK_EQUAL(u8"☗２八銀左引", to_ki2(m28lh, state));
  }
  {
    auto state(csa::read_board("P1+RY *  *  *  *  *  *  *  * \n"
                               "P2 *  *  *  *  *  * -OU *  * \n"
                               "P3 *  *  *  *  *  *  *  *  * \n"
                               "P4 * +RY *  *  *  * +OU *  * \n"
                               "P5 *  *  *  *  *  *  *  *  * \n"
                               "P6 *  *  *  *  *  *  *  *  * \n"
                               "P7 *  *  *  *  *  *  *  *  * \n"
                               "P8 *  *  *  *  *  *  *  *  * \n"
                               "P9 *  *  *  *  *  *  *  *  * \n"
                               "P+00AL\n"
                               "-\n"));
    const Move m82h(Square(9,1), Square(8,2), PROOK, Ptype_EMPTY, false, BLACK);
    const Move m82u(Square(8,4), Square(8,2), PROOK, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗８二龍引", to_ki2(m82h, state));
    TEST_CHECK_EQUAL(u8"☗８二龍上", to_ki2(m82u, state));
  }
  {
    auto state(csa::read_board("P1 *  *  *  *  *  *  *  *  * \n"
                               "P2 *  *  *  * +RY *  *  *  * \n"
                               "P3 *  *  *  *  *  *  * +RY * \n"
                               "P4 *  *  *  *  *  * +OU *  * \n"
                               "P5 *  *  *  *  *  *  *  *  * \n"
                               "P6 *  *  *  *  *  *  *  *  * \n"
                               "P7 *  *  *  *  *  *  *  *  * \n"
                               "P8 *  *  *  *  *  *  *  *  * \n"
                               "P9 *  *  *  *  *  * -OU *  * \n"
                               "P+00AL\n"
                               "-\n"));
    const Move m43y(Square(2,3), Square(4,3), PROOK, Ptype_EMPTY, false, BLACK);
    const Move m43h(Square(5,2), Square(4,3), PROOK, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗４三龍寄", to_ki2(m43y, state));
    TEST_CHECK_EQUAL(u8"☗４三龍引", to_ki2(m43h, state));
  }
  {
    auto state(csa::read_board("P1 *  *  *  *  *  *  *  *  * \n"
                               "P2 *  *  *  *  *  *  *  *  * \n"
                               "P3 *  *  *  * +RY *  * +RY * \n"
                               "P4 *  *  *  *  *  * +OU *  * \n"
                               "P5 *  *  *  *  *  *  *  *  * \n"
                               "P6 *  *  *  *  *  *  *  *  * \n"
                               "P7 *  *  *  *  *  *  *  *  * \n"
                               "P8 *  *  *  *  *  *  *  *  * \n"
                               "P9 *  *  *  *  *  * -OU *  * \n"
                               "P+00AL\n"
                               "-\n"));
    const Move m43r(Square(2,3), Square(4,3), PROOK, Ptype_EMPTY, false, BLACK);
    const Move m43l(Square(5,3), Square(4,3), PROOK, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗４三龍右", to_ki2(m43r, state));
    TEST_CHECK_EQUAL(u8"☗４三龍左", to_ki2(m43l, state));
  }
  {
    auto state(csa::read_board("P1 *  *  *  *  *  *  *  *  * \n"
                               "P2 *  *  *  *  *  *  *  *  * \n"
                               "P3 *  *  *  *  *  *  *  *  * \n"
                               "P4 *  *  *  *  *  * +OU *  * \n"
                               "P5 *  *  *  *  *  *  *  *  * \n"
                               "P6 *  *  *  *  *  *  *  *  * \n"
                               "P7 *  *  *  *  *  *  *  *  * \n"
                               "P8 *  *  *  *  *  *  *  *  * \n"
                               "P9+RY+RY *  *  *  * -OU *  * \n"
                               "P+00AL\n"
                               "-\n"));
    const Move m88l(Square(9,9), Square(8,8), PROOK, Ptype_EMPTY, false, BLACK);
    const Move m88r(Square(8,9), Square(8,8), PROOK, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗８八龍左", to_ki2(m88l, state));
    TEST_CHECK_EQUAL(u8"☗８八龍右", to_ki2(m88r, state));
  }
  {
    auto state(csa::read_board("P1+UM+UM *  *  *  *  *  *  * \n"
                               "P2 *  *  *  *  *  *  *  *  * \n"
                               "P3 *  *  *  *  *  *  *  *  * \n"
                               "P4 *  *  *  *  *  * +OU *  * \n"
                               "P5 *  *  *  * -OU *  *  *  * \n"
                               "P6 *  *  *  *  *  *  *  *  * \n"
                               "P7 *  *  *  *  *  *  *  *  * \n"
                               "P8 *  *  *  *  *  *  * +RY * \n"
                               "P9 *  *  *  *  *  *  *  * +RY\n"
                               "P+00AL\n"
                               "-\n"));
    const Move m17l(Square(2,8), Square(1,7), PROOK, Ptype_EMPTY, false, BLACK);
    const Move m17r(Square(1,9), Square(1,7), PROOK, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗１七龍左", to_ki2(m17l, state));
    TEST_CHECK_EQUAL(u8"☗１七龍右", to_ki2(m17r, state));
    const Move m82l(Square(9,1), Square(8,2), PBISHOP, Ptype_EMPTY, false, BLACK);
    const Move m82r(Square(8,1), Square(8,2), PBISHOP, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗８二馬左", to_ki2(m82l, state));
    TEST_CHECK_EQUAL(u8"☗８二馬右", to_ki2(m82r, state));
  }
  {
    auto state(csa::read_board("P1 *  *  *  *  *  *  *  *  * \n"
                               "P2 *  *  *  *  *  *  *  *  * \n"
                               "P3 *  *  * +UM *  *  *  *  * \n"
                               "P4 *  *  *  *  *  * +OU *  * \n"
                               "P5+UM *  *  * -OU *  *  *  * \n"
                               "P6 *  *  *  *  *  *  *  *  * \n"
                               "P7 *  *  *  *  *  *  *  *  * \n"
                               "P8 *  *  *  *  *  *  *  *  * \n"
                               "P9 *  *  *  *  *  *  *  *  * \n"
                               "P+00AL\n"
                               "-\n"));
    const Move m85y(Square(9,5), Square(8,5), PBISHOP, Ptype_EMPTY, false, BLACK);
    const Move m85h(Square(6,3), Square(8,5), PBISHOP, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗８五馬寄", to_ki2(m85y, state));
    TEST_CHECK_EQUAL(u8"☗８五馬引", to_ki2(m85h, state));
  }
  {
    auto state(csa::read_board(
                               "P1 *  *  *  *  *  *  *  * +UM\n"
                               "P2 *  *  *  *  *  *  *  *  * \n"
                               "P3 *  *  *  *  *  *  *  *  * \n"
                               "P4 *  *  *  *  *  * +UM *  * \n"
                               "P5+OU *  *  * -OU *  *  *  * \n"
                               "P6 *  *  *  *  *  *  *  *  * \n"
                               "P7 *  *  *  *  *  *  *  *  * \n"
                               "P8 *  *  *  *  *  *  *  *  * \n"
                               "P9 *  *  *  *  *  *  *  *  * \n"
                               "P+00AL\n"
                               "-\n"));
    const Move m12h(Square(1,1), Square(1,2), PBISHOP, Ptype_EMPTY, false, BLACK);
    const Move m12u(Square(3,4), Square(1,2), PBISHOP, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗１二馬引", to_ki2(m12h, state));
    TEST_CHECK_EQUAL(u8"☗１二馬上", to_ki2(m12u, state));
  }
  {
    auto state(csa::read_board("P1 *  *  *  *  *  *  *  *  * \n"
                               "P2 *  *  *  *  *  *  *  *  * \n"
                               "P3 *  *  *  *  *  *  *  *  * \n"
                               "P4 *  *  *  *  *  *  *  *  * \n"
                               "P5+OU *  *  * -OU *  *  *  * \n"
                               "P6 *  *  *  *  *  *  *  *  * \n"
                               "P7 *  *  *  *  *  *  *  *  * \n"
                               "P8 *  *  *  *  *  *  *  *  * \n"
                               "P9+UM *  *  * +UM *  *  *  * \n"
                               "P+00AL\n"
                               "-\n"));
    const Move m77l(Square(9,9), Square(7,7), PBISHOP, Ptype_EMPTY, false, BLACK);
    const Move m77r(Square(5,9), Square(7,7), PBISHOP, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗７七馬左", to_ki2(m77l, state));
    TEST_CHECK_EQUAL(u8"☗７七馬右", to_ki2(m77r, state));
  }
  {
    auto state(csa::read_board("P1 *  *  *  *  *  *  *  *  * \n"
                               "P2 *  *  *  *  *  *  *  *  * \n"
                               "P3 *  *  *  *  *  *  *  *  * \n"
                               "P4 *  *  *  *  *  *  *  *  * \n"
                               "P5+OU *  *  * -OU *  *  *  * \n"
                               "P6 *  *  *  *  *  *  *  *  * \n"
                               "P7 *  *  *  *  * +UM *  *  * \n"
                               "P8 *  *  *  *  *  *  *  * +UM\n"
                               "P9 *  *  *  *  *  *  *  *  * \n"
                               "P+00AL\n"
                               "-\n"));
    const Move m29l(Square(4,7), Square(2,9), PBISHOP, Ptype_EMPTY, false, BLACK);
    const Move m29r(Square(1,8), Square(2,9), PBISHOP, Ptype_EMPTY, false, BLACK);
    TEST_CHECK_EQUAL(u8"☗２九馬左", to_ki2(m29l, state));
    TEST_CHECK_EQUAL(u8"☗２九馬右", to_ki2(m29r, state));
  }
  {
    auto state(csa::read_board(
                               "P1 *  *  *  * -OU *  *  *  * \n"
                               "P2 *  *  *  *  *  * +NK *  * \n"
                               "P3 *  *  *  *  *  *  *  *  * \n"
                               "P4 *  *  *  *  *  *  *  *  * \n"
                               "P5 *  *  *  *  *  *  *  *  * \n"
                               "P6 *  *  *  *  *  *  *  *  * \n"
                               "P7 *  *  *  *  *  *  *  *  * \n"
                               "P8 *  *  *  *  *  *  *  *  * \n"
                               "P9 *  *  *  * +OU *  *  *  * \n"
                               "P+00AL\n"
                               "-\n"));
    const Move m33ke(Square(3,3), KNIGHT, BLACK);
    TEST_CHECK_EQUAL(u8"☗３三桂", to_ki2(m33ke, state));
  }
  {
    auto state(csa::read_board("P1+NK * +HI *  *  * -KI-OU-KY\n"
                               "P2 * +FU *  *  *  * -KI-GI * \n"
                               "P3-FU * -GI+UM-FU-FU *  * -FU\n"
                               "P4 *  *  *  *  *  *  *  *  * \n"
                               "P5 *  *  * -KE * -KE *  *  * \n"
                               "P6 *  *  *  * +FU *  *  *  * \n"
                               "P7+FU * +KI+FU * +FU+FU+FU+FU\n"
                               "P8 *  *  *  * +OU+KI * +GI * \n"
                               "P9+KY-HI+GI *  *  *  * +KE+KY\n"
                               "P+00KA00KY00FU00FU\n"
                               "P-00FU00FU00FU00FU\n"
                               "-\n"));
    const Move m57kn(Square(4,5), Square(5,7), PKNIGHT, Ptype_EMPTY, true, WHITE);
    TEST_CHECK_EQUAL(u8"☖５七桂左成", to_ki2(m57kn, state));
  }
  {
    auto state(csa::read_board("P1-KY-KE *  *  *  *  *  * -KY\n"
                               "P2 * -HI *  *  * -KI-OU *  * \n"
                               "P3-FU *  * +TO+FU-KI *  *  * \n"
                               "P4 *  * -FU * -FU-GI * -FU-FU\n"
                               "P5 * -FU *  * -GI-KE-FU *  * \n"
                               "P6 *  * +FU *  * -FU *  * +FU\n"
                               "P7+FU+FU+KE *  *  * +FU+FU * \n"
                               "P8 *  * +KI * +GI * +KI+OU * \n"
                               "P9+KY *  *  * +HI *  * +KE+KY\n"
                               "P+00KA00KA00FU\n"
                               "P-00GI00FU\n"
                               "+\n"));
    const Move m52funari(Square(5,3), Square(5,2), PPAWN, Ptype_EMPTY, true, BLACK);
    TEST_CHECK_EQUAL(u8"☗５二歩成", to_ki2(m52funari, state));
  }
}

namespace osl
{
  MoveVector generate_check_move(const EffectState& state) {
    MoveVector moves;
    MoveStore store(moves);
    auto king = state.kingSquare(alt(state.turn()));
    bool dummy;
    if (state.turn()==BLACK)
      move_generator::AddEffect::generate<BLACK>(state,king,store,dummy);
    else
      move_generator::AddEffect::generate<WHITE>(state,king,store,dummy);
    return moves;
  }
}

void test_addeffect()
{
  {
    auto state = csa::read_board("P1 *  *  * -KI-KI *  * -KE-KY\n"
                                 "P2 * -OU-GI *  * -FU-HI-FU * \n"
                                 "P3 *  *  * -KI+FU *  *  * -FU\n"
                                 "P4 * +FU+FU-FU+KE+GI *  *  * \n"
                                 "P5+FU+KY-FU *  *  *  *  *  * \n"
                                 "P6 * +OU-KE * +RY *  *  *  * \n"
                                 "P7 *  * +KE+FU+KA * +FU * +FU\n"
                                 "P8 *  * +KI *  *  *  *  *  * \n"
                                 "P9 *  *  *  *  *  *  *  * -NG\n"
                                 "P+00KA00FU00FU\n"
                                 "P-00GI00KY00KY00FU00FU00FU00FU\n"
                                 "+\n");

    MoveVector moves = generate_check_move(state);

    Move p83to(Square(8,4),Square(8,3),PPAWN,Ptype_EMPTY,true,BLACK);
    TEST_ASSERT(is_member(moves, p83to));
  }
  
  {
    auto state = csa::read_board("P1-OU * -KI *  *  * +HI * -KY\n"
                                 "P2 *  *  * -KI *  *  *  *  * \n"
                                 "P3-KE-FU *  * -FU-FU+UM * -FU\n"
                                 "P4-FU * -FU+FU-GI *  * -FU * \n"
                                 "P5 *  *  * -FU *  *  *  *  * \n"
                                 "P6 * +FU+FU+GI+FU+FU *  *  * \n"
                                 "P7 * +OU * +KI *  *  *  * +FU\n"
                                 "P8 *  *  * +KI *  *  *  *  * \n"
                                 "P9+KY *  *  *  *  * -RY * +KY\n"
                                 "P+00GI00KE00KE00KY00FU\n"
                                 "P-00KA00GI00KE00FU00FU00FU\n"
                                 "-\n");
    MoveVector moves = generate_check_move(state);
    
    Move p98(Square(9,8),SILVER,WHITE);
    TEST_ASSERT(is_member(moves, p98));
  }
  
  {
    auto state = csa::read_board("P1-OU * -KI *  *  *  *  * -KY\n"
                                 "P2 * -GI *  *  *  *  *  * -HI\n"
                                 "P3-KY-FU * -KI-FU * -KE * -FU\n"
                                 "P4-FU * -FU * -GI *  * -FU * \n"
                                 "P5 *  *  * -FU *  *  *  *  * \n"
                                 "P6 * +FU+FU * +FU+FU * +KA * \n"
                                 "P7 * +GI * +KI+GI *  *  * +FU\n"
                                 "P8 * +OU+KI *  *  *  *  *  * \n"
                                 "P9+KY *  *  *  *  * -HI+KE+KY\n"
                                 "P+00KA00KE00FU00FU\n"
                                 "P-00KE00FU00FU00FU00FU\n"
                                 "+\n");
    MoveVector moves = generate_check_move(state);
    Move p92(Square(9,2),PAWN,BLACK);
    TEST_ASSERT(is_member(moves, p92));
    TEST_ASSERT(state.isCheck(p92));
  }
  {
    auto state = csa::read_board(
                                 "P1 *  * +KA *  *  *  * -KE-KY\n"
                                 "P2-KY *  *  *  *  * -KI *  * \n"
                                 "P3 *  *  *  * +GI * -GI * -FU\n"
                                 "P4-FU * -FU *  *  * +KE *  * \n"
                                 "P5 * -FU *  *  *  * -OU+FU+FU\n"
                                 "P6+FU * +FU * -FU *  *  *  * \n"
                                 "P7 * +FU-KI-RY *  *  *  * +KY\n"
                                 "P8+OU *  *  *  *  *  *  *  * \n"
                                 "P9+KY+KE *  *  *  *  * +KE * \n"
                                 "P+00KI00GI\n"
                                 "P-00HI00KA00KI00GI00FU00FU00FU00FU00FU00FU00FU00FU\n"
                                 "+\n"
                                 );
    auto moves = generate_check_move(state);
    // open move
    TEST_ASSERT(is_member(moves, Move(Square(5,3),Square(4,4),SILVER,Ptype_EMPTY,false,BLACK)));
    TEST_CHECK(! is_member(moves, Move(Square(5,3),Square(4,4),PSILVER,Ptype_EMPTY,true,BLACK)));
  }
  {
    auto state = csa::read_board(
                                 "P1 *  * +KA * -RY *  * -KE-KY\n"
                                 "P2-KY *  *  *  *  * -KI *  * \n"
                                 "P3 *  *  *  * +GI * -GI * -FU\n"
                                 "P4-FU * -FU *  *  * +KE *  * \n"
                                 "P5 * -FU *  *  *  * -OU+FU+FU\n"
                                 "P6+FU * +FU-FU *  *  *  *  * \n"
                                 "P7 * +FU-KI *  *  *  *  * +KY\n"
                                 "P8 *  *  *  *  *  *  *  *  * \n"
                                 "P9+KY+KE *  * +OU *  * +KE * \n"
                                 "P+00KI00GI\n"
                                 "P-00HI00KA00KI00GI00FU00FU00FU00FU00FU00FU00FU00FU\n"
                                 "+\n"
                                 );
    auto moves = generate_check_move(state);
    // open move
    TEST_CHECK(!is_member(moves, Move(Square(5,3),Square(4,4),SILVER,Ptype_EMPTY,false,BLACK)));
  }
  {
    auto state = csa::read_board(
                                 "P1+NY+TO * -FU+GI-KI *  * -KY\n"
                                 "P2 *  *  *  *  *  * -OU * +TO\n"
                                 "P3 * +RY *  *  *  *  * -FU-FU\n"
                                 "P4 *  * +FU+UM *  *  * -GI * \n"
                                 "P5 *  *  *  * +FU+NK-KI+FU+FU\n"
                                 "P6 *  *  * +FU * -FU+KE * +KE\n"
                                 "P7 *  *  *  *  *  *  *  *  *\n"
                                 "P8 *  *  *  *  *  *  *  *  * \n"
                                 "P9 * +OU *  *  *  *  *  * -NG\n"
                                 "P+00HI00KA00KI00GI00KE00KY00FU00FU00FU00FU00FU\n"
                                 "P-00KI00KY00FU00FU\n"
                                 "P-00AL\n"
                                 "+\n"
                                 );
    auto moves = generate_check_move(state);
    // drop move
    TEST_CHECK(is_member(moves, Move(Square(4,4),KNIGHT,BLACK)));
    // move
    TEST_CHECK(is_member(moves, Move(Square(1,6),Square(2,4),KNIGHT,SILVER,false,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(3,6),Square(2,4),KNIGHT,SILVER,false,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(3,6),Square(4,4),KNIGHT,Ptype_EMPTY,false,BLACK)));
    TEST_CHECK(!is_member(moves, Move(Square(4,5),Square(4,4),PKNIGHT,Ptype_EMPTY,false,BLACK)));
  }
  {
    auto state = csa::read_board(
                                 "P1+NY+TO * -FU+GI-KI *  * -KY\n"
                                 "P2 *  *  *  *  *  * -OU * +TO\n"
                                 "P3 * +RY *  *  *  *  * -FU-FU\n"
                                 "P4 *  * +FU+UM *  *  * -GI * \n"
                                 "P5 *  *  *  * +FU+NK-KI+FU+FU\n"
                                 "P6 *  *  * +FU+KE-FU+KE * +KE\n"
                                 "P7 *  *  *  *  *  *  *  *  *\n"
                                 "P8 *  *  *  *  *  *  *  *  * \n"
                                 "P9 * +OU *  *  *  *  *  * -NG\n"
                                 "P+00HI00KA00KI00GI00KY00FU00FU00FU00FU00FU\n"
                                 "P-00KI00KY00FU00FU\n"
                                 "P-00AL\n"
                                 "+\n"
                                 );
    auto moves = generate_check_move(state);
    // move
    TEST_CHECK(is_member(moves, Move(Square(1,6),Square(2,4),KNIGHT,SILVER,false,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(3,6),Square(2,4),KNIGHT,SILVER,false,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(3,6),Square(4,4),KNIGHT,Ptype_EMPTY,false,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(5,6),Square(4,4),KNIGHT,Ptype_EMPTY,false,BLACK)));
    TEST_CHECK(!is_member(moves, Move(Square(4,5),Square(4,4),PKNIGHT,Ptype_EMPTY,false,BLACK)));
  }
  {
    auto state = csa::read_board(
                                 "P1+NY+TO * -FU+GI-KI * -FU-KY\n"
                                 "P2 *  *  *  *  *  * -OU * +FU\n"
                                 "P3 * +RY *  *  *  *  * -KE-FU\n"
                                 "P4 *  * +FU+UM *  *  *  *  *\n"
                                 "P5 *  * -KE * +KA * -KI+FU-GI\n"
                                 "P6+KE *  * +FU+FU-FU *  * +TO\n"
                                 "P7 *  *  *  *  *  *  *  *  *\n"
                                 "P8 *  *  *  *  *  *  *  *  * \n"
                                 "P9 * +OU *  *  *  *  *  * -NG\n"
                                 "P+00HI00KI00GI00KE00KY00FU00FU00FU00FU00FU\n"
                                 "P-00KI00KY00FU00FU\n"
                                 "P-00AL\n"
                                 "+\n"
                                 );
    auto moves = generate_check_move(state);
    // 22の利きをさえぎるので，打ち歩詰めでない
    TEST_CHECK(is_member(moves, Move(Square(3,3),PAWN,BLACK)));
  }
  {
    auto state = csa::read_board(
                                 "P1+RY *  *  *  *  * -KA-KE-KY\n"
                                 "P2 *  *  *  * +TO * -KI * -OU\n"
                                 "P3 *  * -KE *  * -KI+NK-GI * \n"
                                 "P4-FU *  *  * -FU-FU-FU * -FU\n"
                                 "P5 * -FU+FU *  *  *  * +OU * \n"
                                 "P6+FU+FU * +GI * +FU+FU+FU+FU\n"
                                 "P7 *  *  *  *  * +KI * -RY * \n"
                                 "P8+KY-TO *  *  *  *  *  *  * \n"
                                 "P9 * +KE *  *  *  *  * -KI+KY\n"
                                 "P+00KA00GI00KY00FU00FU\n"
                                 "P-00GI00FU\n"
                                 "-\n"
                                 );
    auto moves = generate_check_move(state);
    // drop rook
    TEST_CHECK(!is_member(moves, Move(Square(2,4),PAWN,WHITE)));
  }
  {
    auto state = csa::read_board(
                                 "P1-KY-KE *  *  *  * +RY * -KY\n"
                                 "P2 * -OU *  * +UM * +NK *  * \n"
                                 "P3-FU * -GI-FU-FU-FU *  * -FU\n"
                                 "P4 *  * -FU *  *  *  *  *  * \n"
                                 "P5 *  *  *  * +KA *  *  *  * \n"
                                 "P6 *  *  *  *  *  *  *  *  * \n"
                                 "P7+FU * +FU+FU+FU+FU+FU * +FU\n"
                                 "P8 *  * -NK * +OU *  *  *  * \n"
                                 "P9+KY+KE * -HI * +KI+GI * +KY\n"
                                 "P+00KI00FU00FU00FU\n"
                                 "P-00KI00KI00GI00GI00FU00FU\n"
                                 "+\n"
                                 );
    auto moves = generate_check_move(state);
    // drop rook
    TEST_CHECK(is_member(moves, Move(Square(8,3),PAWN,BLACK)));
  }
  {
    auto state = csa::read_board(
                                 "P1-KY-KE-GI-KI-OU * -GI-KE-KY\n"
                                 "P2 *  *  *  *  *  * -KI *  * \n"
                                 "P3-FU *  * -FU-FU-FU *  * -FU\n"
                                 "P4 *  * -FU *  *  *  *  *  * \n"
                                 "P5 *  *  *  *  *  *  *  *  * \n"
                                 "P6 *  *  *  *  *  *  *  *  * \n"
                                 "P7+FU * +FU+FU+FU+FU+FU * +FU\n"
                                 "P8 * +GI+KI *  *  *  *  *  * \n"
                                 "P9+KY+KE *  * +OU+KI+GI+KE+KY\n"
                                 "P+00HI00KA00FU00FU\n"
                                 "P-00HI00KA00FU00FU00FU\n"
                                 "+\n"
                                 );
    auto moves = generate_check_move(state);
    // drop rook
    TEST_CHECK(is_member(moves, Move(Square(5,2),ROOK,BLACK)));
  }
  {
    auto state = csa::read_board(
                                 "P1+NY+TO * -FU+GI-KI * -KE-KY\n"
                                 "P2 *  *  *  *  *  * -OU * +TO\n"
                                 "P3 * +RY *  *  *  *  * -FU-FU\n"
                                 "P4 *  * +FU+UM *  *  * -GI *\n"
                                 "P5 *  * -KE * +FU * -KI+FU *\n"
                                 "P6+KE *  * +FU * -FU *  * +FU\n"
                                 "P7 *  *  *  *  *  *  *  *  *\n"
                                 "P8 *  *  *  *  *  *  *  *  * \n"
                                 "P9 * +OU *  *  *  *  *  * -NG\n"
                                 "P+00HI00KA00KI00GI00KE00KY00FU00FU00FU00FU00FU\n"
                                 "P-00KI00KY00FU00FU\n"
                                 "P-00AL\n"
                                 "+\n"
                                 );
    auto moves = generate_check_move(state);
    // 打ち歩詰めでない
    TEST_CHECK(is_member(moves, Move(Square(3,3),PAWN,BLACK)));
    // 香車
    TEST_CHECK(is_member(moves, Move(Square(3,3),LANCE,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(3,4),LANCE,BLACK)));
    // 桂馬
    TEST_CHECK(is_member(moves, Move(Square(4,4),KNIGHT,BLACK)));
    // 銀
    TEST_CHECK(is_member(moves, Move(Square(3,3),SILVER,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(4,3),SILVER,BLACK)));
    // 金
    TEST_CHECK(is_member(moves, Move(Square(3,3),GOLD,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(4,3),GOLD,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(2,2),GOLD,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(4,2),GOLD,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(3,1),GOLD,BLACK)));
    // 角
    TEST_CHECK(is_member(moves, Move(Square(4,3),BISHOP,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(5,4),BISHOP,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(6,5),BISHOP,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(7,6),BISHOP,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(8,7),BISHOP,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(9,8),BISHOP,BLACK)));
    // 飛車
    TEST_CHECK(is_member(moves, Move(Square(2,2),ROOK,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(3,1),ROOK,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(3,3),ROOK,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(3,4),ROOK,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(4,2),ROOK,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(5,2),ROOK,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(6,2),ROOK,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(7,2),ROOK,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(8,2),ROOK,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(9,2),ROOK,BLACK)));
  }
  {
    auto state = csa::read_board(
                                 "P1+NY+TO * -FU+GI-KI * -FU-KY\n"
                                 "P2 *  *  *  *  *  * -OU * +TO\n"
                                 "P3 * +RY *  *  *  *  * -KE-FU\n"
                                 "P4 *  * +FU+UM *  *  * +KI *\n"
                                 "P5 *  * -KE * +FU * -GI+FU *\n"
                                 "P6+KE *  * +FU * -FU *  * +FU\n"
                                 "P7 *  *  *  *  *  *  *  *  *\n"
                                 "P8 *  *  *  *  *  *  *  *  * \n"
                                 "P9 * +OU * -GI *  *  *  * -NG\n"
                                 "P+00HI00KA00KI00KE00KY00FU00FU00FU00FU00FU\n"
                                 "P-00KI00KY00FU00FU\n"
                                 "P-00AL\n"
                                 "+\n"
                                 );
    auto moves = generate_check_move(state);
    // 打ち歩詰めは生成しない
    TEST_CHECK(!is_member(moves, Move(Square(3,3),PAWN,BLACK)));
    
    
  }
  {
    auto state = csa::read_board(
                                 "P1+NY+TO * -FU+GI-KI * -KE-KY\n"
                                 "P2 *  *  *  *  *  * -OU * +FU\n"
                                 "P3 * +RY *  *  *  *  * -FU-FU\n"
                                 "P4 *  * +FU+UM *  *  * -GI *\n"
                                 "P5 *  * -KE * +KA * -KI+FU *\n"
                                 "P6+KE *  * +FU+FU-FU *  * +TO\n"
                                 "P7 *  *  *  *  *  *  *  *  *\n"
                                 "P8 *  *  *  *  *  *  *  *  * \n"
                                 "P9 * +OU *  *  *  *  *  * -NG\n"
                                 "P+00HI00KI00GI00KE00KY00FU00FU00FU00FU00FU\n"
                                 "P-00KI00KY00FU00FU\n"
                                 "P-00AL\n"
                                 "+\n"
                                 );
    auto moves = generate_check_move(state);
    // 取り返せるので，打ち歩詰めでない
    TEST_CHECK(is_member(moves, Move(Square(3,3),PAWN,BLACK)));
  }
  {
    auto state=csa::read_board(
                               "P1-KY-KE * -KI *  * +GI-KE-KY\n"
                               "P2 *  *  * +RY *  * +HI *  * \n"
                               "P3-FU * -FU *  * +FU *  *  * \n"
                               "P4 * -FU *  * -OU *  *  * +KA\n"
                               "P5 *  *  *  * -GI *  *  * -FU \n"
                               "P6 *  * +FU+FU *  *  *  *  * \n"
                               "P7+FU+FU *  *  *  *  *  * +FU\n"
                               "P8 *  * +KI *  *  * +GI+UM-GI\n"
                               "P9+KY+KE * +OU+KI *  * +KE+KY\n"
                               "P+00FU00FU\n"
                               "P-00FU00FU00FU\n"
                               "P-00FU\n"
                               "P-00FU\n"
                               "P-00FU\n"
                               "P+00KI\n"
                               "+\n");
    auto moves = generate_check_move(state);
    // prook
    TEST_CHECK(is_member(moves,Move(Square(6,2),Square(5,1),PROOK,Ptype_EMPTY,false,BLACK)));
    TEST_CHECK(is_member(moves,Move(Square(6,2),Square(5,2),PROOK,Ptype_EMPTY,false,BLACK)));
    TEST_ASSERT(is_member(moves,Move(Square(6,2),Square(5,3),PROOK,Ptype_EMPTY,false,BLACK)));
    TEST_CHECK(is_member(moves,Move(Square(6,2),Square(6,3),PROOK,Ptype_EMPTY,false,BLACK)));
    TEST_CHECK(is_member(moves,Move(Square(6,2),Square(6,4),PROOK,Ptype_EMPTY,false,BLACK)));
    TEST_CHECK(is_member(moves,Move(Square(6,2),Square(6,5),PROOK,Ptype_EMPTY,false,BLACK)));
    // BISHOP
    TEST_CHECK(is_member(moves,Move(Square(1,4),Square(3,6),BISHOP,Ptype_EMPTY,false,BLACK)));
    // PBISHOP
    TEST_CHECK(is_member(moves,Move(Square(2,8),Square(1,8),PBISHOP,SILVER,false,BLACK)));
    TEST_CHECK(is_member(moves,Move(Square(2,8),Square(2,7),PBISHOP,Ptype_EMPTY,false,BLACK)));
    TEST_CHECK(is_member(moves,Move(Square(2,8),Square(5,5),PBISHOP,SILVER,false,BLACK)));
  }
  {
    auto state=csa::read_board(
                               "P1-KY-KE * +RY *  *  *  *  * \n"
                               "P2-OU * -GI *  *  *  *  *  * \n"
                               "P3 *  * -UM-FU-FU *  * -FU * \n"
                               "P4-FU-FU-FU *  *  *  *  * -FU\n"
                               "P5 *  *  * -KY * -FU * +FU * \n"
                               "P6+FU * +FU+FU *  *  *  * +FU\n"
                               "P7 * +FU+GI-KE+UM+FU *  *  * \n"
                               "P8+KY+GI+KI *  *  *  *  *  * \n"
                               "P9+OU+KE *  *  *  *  * -RY * \n"
                               "P+00KI00KI00KY\n"
                               "P-00KI00GI00KE00FU00FU00FU\n"
                               "+\n"
                               );
    auto moves = generate_check_move(state);
    TEST_CHECK(!is_member(moves,Move(Square(5,7),Square(7,4),PBISHOP,PAWN,false,BLACK)));
  }
  {
    auto state=csa::read_board(
                               "P1-KY-KE * -KY+RY *  *  *  * \n"
                               "P2 *  * -OU *  *  *  *  *  * \n"
                               "P3 *  * -FU-KI * +TO * -FU * \n"
                               "P4-FU *  *  * -FU *  *  * +FU\n"
                               "P5 * +KE * -KA *  * -FU+FU * \n"
                               "P6+FU * +KY * +FU *  *  *  * \n"
                               "P7 * +OU * +FU+KA * +FU *  * \n"
                               "P8 *  *  * +GI *  *  *  *  * \n"
                               "P9+KY *  *  * +KI *  *  * -RY\n"
                               "P+00KI00GI00FU00FU\n"
                               "P-00KI00GI00GI00KE00KE00FU00FU00FU00FU\n"
                               "+\n"
                               );
    auto moves = generate_check_move(state);
    TEST_CHECK(!is_member(moves,Move(Square(7,6),Square(7,3),PLANCE,PAWN,true,BLACK)));
  }
  {
    auto state=csa::read_board(
                               "P1-KY-KE-GI-KI-OU * -GI-KE-KY\n"
                               "P2 * -HI *  *  *  * -KI-KA * \n"
                               "P3-FU * -FU-FU-FU-FU *  * -FU\n"
                               "P4 *  *  *  *  *  * -FU+HI * \n"
                               "P5 *  *  *  *  *  *  *  *  * \n"
                               "P6 * -FU+FU *  *  *  *  *  * \n"
                               "P7+FU+FU * +FU+FU+FU+FU * +FU\n"
                               "P8 * +KA+KI *  *  *  *  *  * \n"
                               "P9+KY+KE+GI * +OU+KI+GI+KE+KY\n"
                               "P+00FU\n"
                               "P-00FU\n"
                               "+\n"
                               );
    auto moves = generate_check_move(state);
    TEST_CHECK(!is_member(moves,Move(Square(2,4),Square(2,3),PROOK,Ptype_EMPTY,true,BLACK)));
  }

  {
    auto state=csa::read_board(
                               "P1-KY-KE *  *  * -OU * -KE-KY\n"
                               "P2 *  *  *  *  *  * -KI-KA * \n"
                               "P3-FU * -FU-FU+HI-GI * +RY * \n"
                               "P4 * -FU *  *  *  * +FU-FU-FU\n"
                               "P5 *  *  *  * -GI *  *  *  * \n"
                               "P6 *  * +FU+FU * +KY *  *  * \n"
                               "P7+FU+FU+GI *  * +FU+GI * +FU\n"
                               "P8 *  * +KI * +KI *  *  *  * \n"
                               "P9+KY+KE * +OU *  *  * +KE * \n"
                               "P+00FU\n"
                               "P-00FU\n"
                               "P-00FU\n"
                               "P-00FU\n"
                               "P-00FU\n"
                               "P+00KI\n"
                               "P-00KA\n"
                               "+\n");
    auto moves = generate_check_move(state);
    TEST_CHECK(is_member(moves,Move(Square(2,3),Square(4,3),PROOK,SILVER,false,BLACK)));
    TEST_CHECK(is_member(moves,Move(Square(5,3),Square(4,3),PROOK,SILVER,true,BLACK)));
    TEST_CHECK(is_member(moves,Move(Square(4,6),Square(4,3),LANCE,SILVER,false,BLACK)));
    TEST_CHECK(!is_member(moves,Move(Square(4,6),Square(4,3),PLANCE,SILVER,true,BLACK)));
  }
  {
    auto state=csa::read_board(
                               "P1-KY-KE * -KI * -OU+GI-KE-KY\n"
                               "P2 *  * +RY *  *  * +HI *  * \n"
                               "P3-FU * -FU-FU+KI+FU *  *  * \n"
                               "P4 * -FU *  * +KE-FU+FU * -FU\n"
                               "P5 *  *  *  * -GI-GI *  * +KA\n"
                               "P6 *  * +FU+FU *  *  *  *  * \n"
                               "P7+FU+FU *  *  *  * +GI * +FU\n"
                               "P8 *  * +KI *  *  *  *  *  * \n"
                               "P9+KY+KE * +OU *  *  *  * +KY\n"
                               "P+00FU\n"
                               "P-00FU\n"
                               "P-00FU\n"
                               "P-00FU\n"
                               "P-00FU\n"
                               "P+00KI\n"
                               "P-00KA\n"
                               "+\n");
    auto moves = generate_check_move(state);
    // 龍
    // pawn
    TEST_CHECK(is_member(moves,Move(Square(4,3),Square(4,2),PPAWN,Ptype_EMPTY,true,BLACK)));
    // silver
    TEST_CHECK(is_member(moves,Move(Square(3,1),Square(4,2),SILVER,Ptype_EMPTY,false,BLACK)));
    TEST_CHECK(is_member(moves,Move(Square(3,1),Square(4,2),PSILVER,Ptype_EMPTY,true,BLACK)));
    // KNIGHT
    TEST_CHECK(is_member(moves,Move(Square(5,4),Square(4,2),PKNIGHT,Ptype_EMPTY,true,BLACK)));
    // GOLD
    TEST_CHECK(is_member(moves,Move(Square(5,3),Square(4,2),GOLD,Ptype_EMPTY,false,BLACK)));
    // ROOK
    TEST_CHECK(is_member(moves,Move(Square(3,2),Square(4,2),PROOK,Ptype_EMPTY,true,BLACK)));
    // PROOK
    TEST_CHECK(is_member(moves,Move(Square(7,2),Square(4,2),PROOK,Ptype_EMPTY,false,BLACK)));
  }
  {
    auto state=csa::read_board(
                               "P1-KY-KE * -KI *  * +GI-KE-KY\n"
                               "P2 *  *  *  *  *  *  *  *  * \n"
                               "P3-FU+RY-FU *  * +FU *  *  * \n"
                               "P4 * +FU *  * -OU *  *  * +KA\n"
                               "P5 *  *  *  *  *  *  *  * -FU \n"
                               "P6 *  * +FU+FU *  *  *  *  * \n"
                               "P7+FU-FU *  * -GI+RY *  * +FU\n"
                               "P8 *  * +KI *  *  * +GI+UM-GI\n"
                               "P9+KY+KE * +OU+KI *  * +KE+KY\n"
                               "P+00FU00FU\n"
                               "P-00FU00FU00FU\n"
                               "P-00FU\n"
                               "P-00FU\n"
                               "P-00FU\n"
                               "P+00KI\n"
                               "+\n");
    auto moves = generate_check_move(state);
    // 47 PROOK
    TEST_CHECK(is_member(moves,Move(Square(4,7),Square(4,4),PROOK,Ptype_EMPTY,false,BLACK)));
    TEST_CHECK(is_member(moves,Move(Square(4,7),Square(4,5),PROOK,Ptype_EMPTY,false,BLACK)));
    TEST_CHECK(is_member(moves,Move(Square(4,7),Square(5,6),PROOK,Ptype_EMPTY,false,BLACK)));
    TEST_CHECK(is_member(moves,Move(Square(4,7),Square(5,7),PROOK,SILVER,false,BLACK)));
    TEST_CHECK(!is_member(moves,Move(Square(4,7),Square(5,8),PROOK,Ptype_EMPTY,false,BLACK)));

    // 83 PROOK
    TEST_CHECK(is_member(moves,Move(Square(8,3),Square(7,4),PROOK,Ptype_EMPTY,false,BLACK)));
    TEST_CHECK(!is_member(moves,Move(Square(8,3),Square(9,4),PROOK,Ptype_EMPTY,false,BLACK)));
  }
  {
    auto state=csa::read_board(
                               "P1+NY+TO *  *  *  * -OU-KE-KY\n"
                               "P2 *  *  *  *  * -GI-KI *  *\n"
                               "P3 * +RY *  * +UM * -KI-FU-FU\n"
                               "P4 *  * +FU-FU *  *  *  *  *\n"
                               "P5 *  * -KE * +FU *  * +FU *\n"
                               "P6+KE *  * +FU+GI-FU *  * +FU\n"
                               "P7 *  * -UM *  *  *  *  *  *\n"
                               "P8 *  *  *  *  *  *  *  *  * \n"
                               "P9 * +OU * -GI *  *  *  * -NG\n"
                               "P+00HI00KI00KE00KY00FU00FU00FU00FU00FU00FU\n"
                               "P-00KI00KY00FU00FU\n"
                               "P-00AL\n"
                               "+\n"
                               );
    auto moves = generate_check_move(state);
    TEST_CHECK(is_member(moves,Move(Square(5,3),Square(4,2),PBISHOP,SILVER,false,BLACK)));
  }
  {
    auto state= csa::read_board(
                                "P1-KY-KE-OU+KA *  * +RY * -KY\n"
                                "P2 *  *  *  * -KI * +NK *  * \n"
                                "P3-FU * -GI-FU-FU-FU *  * -FU\n"
                                "P4 *  * -FU *  *  *  *  *  * \n"
                                "P5 *  *  *  * +KA *  *  *  * \n"
                                "P6 *  *  *  *  *  *  *  *  * \n"
                                "P7+FU * +FU+FU+FU+FU+FU * +FU\n"
                                "P8 *  * -NK * +OU *  *  *  * \n"
                                "P9+KY+KE * -HI * +KI+GI * +KY\n"
                                "P+00FU00FU00FU\n"
                                "P-00KI00KI00GI00GI00FU00FU\n"
                                "+\n"
                                );
    auto moves = generate_check_move(state);
    // open move
    TEST_CHECK(is_member(moves,Move(Square(6,1),Square(5,2),PBISHOP,GOLD,true,BLACK)));
  }
  {
    auto state=csa::read_board(
                               "P1-KY-KE-GI-KI * -KI-GI-KE-KY\n"
                               "P2 * -HI *  *  *  *  * -KA * \n"
                               "P3-FU-FU-FU-FU-FU-FU-FU-FU-FU\n"
                               "P4 *  *  *  * -OU *  *  *  * \n"
                               "P5 *  *  *  * +KE *  *  *  * \n"
                               "P6 *  * +FU *  *  *  *  *  * \n"
                               "P7+FU+FU * +FU+KY+FU+FU+FU+FU\n"
                               "P8 * +KA *  * +FU *  * +HI * \n"
                               "P9+KY+KE+GI+KI+OU+KI+GI *  * \n"
                               "+\n"
                               );
    auto moves = generate_check_move(state);
    TEST_CHECK(is_member(moves,Move(Square(5,5),Square(4,3),KNIGHT,PAWN,false,BLACK)));
    TEST_CHECK(is_member(moves,Move(Square(5,5),Square(4,3),PKNIGHT,PAWN,true,BLACK)));
    TEST_CHECK(is_member(moves,Move(Square(5,5),Square(6,3),KNIGHT,PAWN,false,BLACK)));
    TEST_CHECK(is_member(moves,Move(Square(5,5),Square(6,3),PKNIGHT,PAWN,true,BLACK)));
  }
  {
    auto state=csa::read_board(
                               "P1-KY-KE-GI-KI * -KI-GI-KE-KY\n"
                               "P2 * -HI *  * -FU *  * -KA * \n"
                               "P3-FU-FU-FU-FU-OU-FU-FU-FU-FU\n"
                               "P4 *  *  *  *  *  *  *  *  * \n"
                               "P5 *  *  *  * +KE *  *  *  * \n"
                               "P6 *  * +FU *  *  *  *  *  * \n"
                               "P7+FU+FU * +FU+KY+FU+FU+FU+FU\n"
                               "P8 * +KA *  * +FU *  * +HI * \n"
                               "P9+KY+KE+GI+KI+OU+KI+GI *  * \n"
                               "+\n"
                               );
    auto moves = generate_check_move(state);
    TEST_CHECK(is_member(moves,Move(Square(5,5),Square(4,3),KNIGHT,PAWN,false,BLACK)));
    TEST_CHECK(is_member(moves,Move(Square(5,5),Square(6,3),KNIGHT,PAWN,false,BLACK)));
  }
  {
    // actually, 98KY is not a pinned piece
    auto state=csa::read_board(
                               "P1-OU *  * -KI *  *  *  * -KY\n"
                               "P2 *  * -GI * -KI-HI *  *  * \n"
                               "P3 * -FU *  * -FU-GI-KE * -FU\n"
                               "P4 *  * -FU-FU * -KA-FU+HI * \n"
                               "P5 * +KE *  *  * -FU *  *  * \n"
                               "P6 *  * +FU * +FU * +FU *  * \n"
                               "P7-KY+FU * +FU * +FU *  * +FU\n"
                               "P8+KY+GI *  * +KI+GI *  *  * \n"
                               "P9+OU *  * +KI *  *  * +KE+KY\n"
                               "P+00KA00FU00FU\n"
                               "P-00KE00FU00FU\n"
                               "+\n"
                               );
    auto moves = generate_check_move(state);
    TEST_CHECK(is_member(moves, Move(Square(9,8),Square(9,7),LANCE,LANCE,false,BLACK)));
  }
  {
    // 88KY is a pinned piece
    auto state=csa::read_board(
                               "P1-KY-OU *  * -KI *  *  *  * \n"
                               "P2 *  *  * -GI * -KI-HI *  * \n"
                               "P3-FU * -FU *  * -FU-GI-KE * \n"
                               "P4 *  *  * -FU-FU * -KA-FU+HI\n"
                               "P5 *  * +KE *  *  * -FU *  * \n"
                               "P6 *  * +FU+FU * +FU * +FU * \n"
                               "P7+FU-KY-KA * +FU * +FU *  * \n"
                               "P8 * +KY+GI *  * +KI+GI *  * \n"
                               "P9+OU+KY *  * +KI *  *  * +KE\n"
                               "P+00FU00FU\n"
                               "P-00KE00FU00FU\n"
                               "+\n"
                               );
    auto moves = generate_check_move(state);
    TEST_CHECK(!is_member(moves, Move(Square(8,8),Square(8,7),LANCE,LANCE,false,BLACK)));
  }
  {
    // 88KY is a pinned piece
    auto state=csa::read_board(
                               "P1-KY-OU *  * -KI *  *  *  * \n"
                               "P2 *  *  * -GI * -KI *  *  * \n"
                               "P3-FU * -FU *  * -FU-GI-KE * \n"
                               "P4 *  *  * -FU-FU * -KA-FU+HI\n"
                               "P5 *  * +KE *  *  * -FU *  * \n"
                               "P6 *  * +FU+FU * +FU * +FU * \n"
                               "P7+FU-KY-KA * +FU * +FU *  * \n"
                               "P8+OU+KY-HI *  * +KI+GI *  * \n"
                               "P9 * +KY+GI * +KI *  *  * +KE\n"
                               "P+00FU00FU\n"
                               "P-00KE00FU00FU\n"
                               "+\n"
                               );
    auto moves = generate_check_move(state);
    TEST_CHECK(!is_member(moves, Move(Square(8,8),Square(8,7),LANCE,LANCE,false,BLACK)));
  }
  // ROOK (attack king)
  {
    // 
    auto state=csa::read_board(
                               "P1 *  *  *  *  *  *  *  *  * \n"
                               "P2 *  *  *  *  *  *  *  *  * \n"
                               "P3 *  *  *  * -OU * -KY *  * \n"
                               "P4 *  *  *  *  *  *  *  *  * \n"
                               "P5 *  *  *  *  *  * +HI *  * \n"
                               "P6 *  *  *  *  *  * +OU *  * \n"
                               "P7 *  *  *  *  *  *  *  *  * \n"
                               "P8 *  *  *  *  *  *  *  *  * \n"
                               "P9 *  *  *  *  *  *  *  *  * \n"
                               "P+00FU00FU\n"
                               "P-00AL\n"
                               "+\n"
                               );
    auto moves = generate_check_move(state);
    TEST_CHECK(!is_member(moves, Move(Square(3,5),Square(5,5),ROOK,Ptype_EMPTY,false,BLACK)));
  }
  {
    // 
    auto state=csa::read_board(
                               "P1 *  *  *  *  *  *  *  *  * \n"
                               "P2 *  *  *  *  *  *  *  *  * \n"
                               "P3 *  *  *  * -OU *  *  *  * \n"
                               "P4 *  *  *  *  *  *  * -KA * \n"
                               "P5 *  *  *  *  *  * +HI *  * \n"
                               "P6 *  *  *  *  * +OU *  *  * \n"
                               "P7 *  *  *  *  *  *  *  *  * \n"
                               "P8 *  *  *  *  *  *  *  *  * \n"
                               "P9 *  *  *  *  *  *  *  *  * \n"
                               "P+00FU00FU\n"
                               "P-00AL\n"
                               "+\n"
                               );
    auto moves = generate_check_move(state);
    TEST_CHECK(!is_member(moves, Move(Square(3,5),Square(5,5),ROOK,Ptype_EMPTY,false,BLACK)));
  }
  {
    // 
    auto state=csa::read_board(
                               "P1 *  *  *  *  *  *  *  *  * \n"
                               "P2 *  *  *  *  *  *  *  *  * \n"
                               "P3 *  *  *  * -OU *  *  *  * \n"
                               "P4 *  *  *  *  *  *  *  *  * \n"
                               "P5 *  *  * -HI *  * +HI+OU * \n"
                               "P6 *  *  *  *  *  *  *  * \n"
                               "P7 *  *  *  *  *  *  *  *  * \n"
                               "P8 *  *  *  *  *  *  *  *  * \n"
                               "P9 *  *  *  *  *  *  *  *  * \n"
                               "P+00FU00FU\n"
                               "P-00AL\n"
                               "+\n"
                               );
    auto moves = generate_check_move(state);
    TEST_CHECK(is_member(moves, Move(Square(3,5),Square(5,5),ROOK,Ptype_EMPTY,false,BLACK)));
  }
  {
    // 
    auto state=csa::read_board(
                               "P1 *  *  *  *  *  *  *  *  * \n"
                               "P2 *  *  *  *  *  *  *  *  * \n"
                               "P3 *  *  *  * -OU *  *  *  * \n"
                               "P4 *  *  *  *  *  *  *  *  * \n"
                               "P5 *  *  *  * -HI * +HI+OU * \n"
                               "P6 *  *  *  *  *  *  *  * \n"
                               "P7 *  *  *  *  *  *  *  *  * \n"
                               "P8 *  *  *  *  *  *  *  *  * \n"
                               "P9 *  *  *  *  *  *  *  *  * \n"
                               "P+00FU00FU\n"
                               "P-00AL\n"
                               "+\n"
                               );
    auto moves = generate_check_move(state);
    TEST_CHECK(is_member(moves, Move(Square(3,5),Square(5,5),ROOK,ROOK,false,BLACK)));
  }
  {
    // 
    auto state=csa::read_board(
                               "P1 *  *  *  *  *  *  *  *  * \n"
                               "P2 *  *  *  *  *  *  *  *  * \n"
                               "P3 *  *  *  *  *  *  *  * \n"
                               "P4 *  *  *  * -OU *  *  *  * \n"
                               "P5 *  *  *  *  *  *  *  *  * \n"
                               "P6 *  * -HI *  *  * +HI+OU *  * \n"
                               "P7 *  *  *  *  *  *  *  *  * \n"
                               "P8 *  *  *  *  *  *  *  *  * \n"
                               "P9 *  *  *  *  *  *  *  *  * \n"
                               "P+00FU00FU\n"
                               "P-00AL\n"
                               "+\n"
                               );
    auto moves = generate_check_move(state);
    TEST_CHECK(!is_member(moves, Move(Square(3,6),Square(3,4),ROOK,Ptype_EMPTY,false,BLACK)));
  }
  {
    // 
    auto state=csa::read_board(
                               "P1 *  *  *  *  *  *  *  *  * \n"
                               "P2 *  *  *  *  *  *  *  *  * \n"
                               "P3 *  *  *  *  *  * -HI *  * \n"
                               "P4 *  *  *  * -OU *  *  *  * \n"
                               "P5 *  *  *  *  *  *  *  *  * \n"
                               "P6 *  *  *  *  *  * +HI *  *  * \n"
                               "P7 *  *  *  *  *  * +OU *  * \n"
                               "P8 *  *  *  *  *  *  *  *  * \n"
                               "P9 *  *  *  *  *  *  *  *  * \n"
                               "P+00FU00FU\n"
                               "P-00AL\n"
                               "+\n"
                               );
    auto moves = generate_check_move(state);
    TEST_CHECK(is_member(moves, Move(Square(3,6),Square(3,4),ROOK,Ptype_EMPTY,false,BLACK)));
  }
  {
    // 
    auto state=csa::read_board(
                               "P1 *  *  *  *  *  *  *  *  * \n"
                               "P2 *  *  *  *  *  * -KY *  * \n"
                               "P3 *  *  * -OU *  *  *  *  * \n"
                               "P4 *  *  *  *  *  * +RY *  * \n"
                               "P5 *  *  *  *  *  *  *  *  * \n"
                               "P6 *  *  *  *  *  * +OU *  * \n"
                               "P7 *  *  *  *  *  *  *  *  * \n"
                               "P8 *  *  *  *  *  *  *  *  * \n"
                               "P9 *  *  *  *  *  *  *  *  * \n"
                               "P+00FU00FU\n"
                               "P-00AL\n"
                               "+\n"
                               );
    auto moves = generate_check_move(state);
    TEST_CHECK(!is_member(moves, Move(Square(3,4),Square(4,3),PROOK,Ptype_EMPTY,false,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(3,4),Square(3,3),PROOK,Ptype_EMPTY,false,BLACK)));
    TEST_CHECK(!is_member(moves, Move(Square(3,4),Square(2,3),PROOK,Ptype_EMPTY,false,BLACK)));
  }
  {
    auto state=csa::read_board(
                               "P1-KY-KE *  *  * +KA-KY *  * \n"
                               "P2 * -HI *  * +FU * -KE *  * \n"
                               "P3 *  * -FU-GI-FU *  * -OU-KI\n"
                               "P4-FU *  * -FU * -FU-GI *  * \n"
                               "P5 * -FU+FU *  *  *  *  * -KY\n"
                               "P6 *  *  *  *  *  * -FU *  * \n"
                               "P7+FU+FU+KE+FU * +FU+FU+FU+HI\n"
                               "P8 * +GI *  * +KI *  *  *  * \n"
                               "P9+KY *  *  *  *  *  *  * +OU\n"
                               "P+00KA00KI00KE\n"
                               "P-00KI00GI00FU00FU00FU\n"
                               "-\n"
                               );
    auto moves = generate_check_move(state);
    // open move
    TEST_CHECK(is_member(moves, Move(Square(1,5),Square(1,7),LANCE,ROOK,false,WHITE)));
    TEST_CHECK(!is_member(moves, Move(Square(1,5),Square(1,7),PLANCE,ROOK,true,WHITE)));
  }
}



TEST_LIST = {
  { "player", test_player },
  { "ptype", test_ptype },
  { "direction", test_direction },
  { "square", test_square },
  { "piece_stand", test_piece_stand },
  { "move", test_move },
  { "offset", test_offset },
  { "king8", test_king8 },
  { "state", test_state },
  { "effect_state", test_effect_state },
  { "effect_state2", test_effect_state2 },
  { "usi", test_usi },
  { "ki2", test_ki2 },
  { "classifier", test_classify },
  { "checkmate", test_checkmate },
  { "addeffect", test_addeffect },
  { "movegen", test_movegen },
  { nullptr, nullptr }
};
