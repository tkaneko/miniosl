// immediateCheckmate.tcc
// immediateCheckmate.cc
#include "checkmate.h"
#include "state.h"
#include "more.h"
#include <iostream>
namespace osl
{
  namespace checkmate
  {
    namespace detail {
      template<Player P>
      bool blockingVerticalAttack(EffectState const& state,Square pos)
      {
	PieceMask effect=state.effectAt(pos)&
	  state.effectAt(pos+to_offset(P,U));
	mask_t mask=effect.to_ullong();
	mask&=(state.piecesOnBoard(P).to_ullong()<<8);
	if((mask&mask_t(piece_id_set(LANCE)<<8)) == 0){
	  mask&=mask_t(piece_id_set(ROOK)<<8);
          for (int num: long_to_piece_id_range(mask)) {
	    Square from=state.pieceOf(num).square();
	    assert(from.isOnBoard());
	    if(from.isU(P, pos)) goto found;
	  }
	  return false;
	found:;
	}
	constexpr Offset offset=to_offset(P,U);
	pos+=offset;
	constexpr Player altP=alt(P);
	for(int i=0;i<3;i++,pos+=offset){
	  Piece p=state.pieceAt(pos);
	  if(p.canMoveOn<altP>()){ // 自分の駒か空白
	    if(state.countEffect(P,pos)==1) return true;
	    if(!p.isEmpty()) return false;
	  }
	  else return false;
	}
	return false;
      }
      template<Player P>
      bool blockingDiagonalAttack(EffectState const& state,Square pos,Square target,
			     King8Info canMoveMask)
      {
	const Player altP=alt(P);
	Square to=target-to_offset(P,U);
	// Uに相手の駒がある
	if((canMoveMask&(0x10000<<Int(U)))==0) return false;
	PieceMask effect=state.effectAt(to)&state.effectAt(pos);
	mask_t mask=effect.to_ullong();  // longは常に1
	mask&=(state.piecesOnBoard(P).to_ullong()<<8);
	mask&=mask_t(piece_id_set(BISHOP)<<8);
        for (int num: long_to_piece_id_range(mask)) {
	  Square from=state.pieceOf(num).square();
	  assert(from.isOnBoard());
	  // Offset offset=base8_step_unsafe(to,from);
	  Offset offset=base8_step(from,to);
	  if(to+offset != pos) continue;
	  if(state.countEffect(P,to)==1) return true;
	  // Uがspaceだと絡んでくる
	  if(!state.pieceAt(to).isEmpty()) return false;
	  Square pos1=to-offset;
	  // BISHOPの利き一つで止めていた
	  Piece p=state.pieceAt(pos1);
	  if(p.canMoveOn<altP>() &&
	     state.countEffect(P,pos1)==1){
	    return true;
	  }
	}
	return false;
      }
      template<Player P,bool canDrop>
      bool hasKnightCheckmate(EffectState const& state, 
			      Square target, 
			      Square pos,
			      King8Info canMoveMask,
			      Move& win_move, mask_t mask1)
      {
	if(!pos.isOnBoard()) return false;
	const Player altP=alt(P);
	Piece p=state.pieceAt(pos);
	if(p.canMoveOn<P>() && 
	   !state.hasEffectByNotPinned(altP,pos)
	  ){
	  mask_t mask=state.effectAt(pos).to_ullong()&mask1;
	  if(mask){
	    if(blockingVerticalAttack<P>(state,pos) ||
	       blockingDiagonalAttack<P>(state,pos,target,canMoveMask))
              return false;
            Piece p1=state.pieceOf(take_one_bit(mask));
            win_move=Move(p1.square(),pos,KNIGHT,p.ptype(),false,P);
	    return true;
	  }
	  else if(canDrop && p.isEmpty()){
	    if(blockingVerticalAttack<P>(state,pos) ||
	       blockingDiagonalAttack<P>(state,pos,target,canMoveMask)) return false;
            win_move=Move(pos,KNIGHT,P);
	    return true;
	  }
	}
	return false;
      }
      // KNIGHT
      // KNIGHTのdropは利きを遮ることはない
      template<Player P>
      bool hasCheckmateMoveKnight(EffectState const& state, Square target, 
				  King8Info canMoveMask,Move& win_move)
      {
	// 8近傍に移動できる時は桂馬の一手詰めはない
	if((canMoveMask&0xff00)!=0) return false;
	mask_t mask= piece_id_set(KNIGHT);
	mask&=state.piecesOnBoard(P).to_ullong();
	mask&= ~state.promotedPieces().to_ullong();
	mask&= ~state.pinOrOpen(P).to_ullong();
	if(state.hasPieceOnStand<KNIGHT>(P)){
	  Square pos=target-to_offset(P,UUR);
	  if(hasKnightCheckmate<P,true>(state,target,pos,canMoveMask,win_move,mask))
	    return true;
	  pos=target-to_offset(P,UUL);
	  return hasKnightCheckmate<P,true>(state,target,pos,canMoveMask,win_move,mask);
	}
	else{
	  Square pos=target-to_offset(P,UUR);
	  if(hasKnightCheckmate<P,false>(state,target,pos,canMoveMask,win_move,mask))
	    return true;
	  pos=target-to_offset(P,UUL);
	  return hasKnightCheckmate<P,false>(state,target,pos,canMoveMask,win_move,mask);
	}
	return false;
      }
      template<Player P>
      bool slowCheckDrop(EffectState const& state,Square target,
			 Ptype ptype,King8Info canMoveMask,Move& win_move)
      {
	mask_t dropMask= dropCandidate(canMoveMask)
	  &Immediate_Checkmate_Table.ptypeDropMask(ptype,canMoveMask);
	// dropMaskが0ならここに来ない
	assert(dropMask!=0);
        for (int i: to_range(dropMask)) {
	  Direction d=static_cast<Direction>(i);
	  mask_t blockingMask=Immediate_Checkmate_Table.blockingMask(ptype,d) &
	    (canMoveMask>>16);
	  Square drop=target-to_offset<P>(d);
	  if(blockingMask!=0){
	    EffectPieceMask effect=state.effectAt(drop);
	    mask_t longEffect=effect.selectLong();
	    longEffect&=(state.piecesOnBoard(P).to_ullong()<<8);
	    if(longEffect){
	      for (int j: to_range(blockingMask)) {
		Direction d1=static_cast<Direction>(j);
		Square pos=target-to_offset<P>(d1);
		EffectPieceMask effect1=state.effectAt(pos);
		if(effect1.countEffect(P)>1) continue;
		mask_t longEffect1=effect1.to_ullong()&longEffect;
		if(!longEffect1) continue;
		//
		int num=take_one_bit(longEffect1)+EffectPieceMask::longToNumOffset;
		if(is_between_unsafe(drop,state.pieceOf(num).square(),pos))
		  goto tryNext;
	      }
	    }
	  }
	  // blockingMaskの点がすべてOKならOK
          win_move=Move(drop,ptype,P);
	  return true;
	tryNext:;
	}
	return false;
      }
    } // detail
  } // checkmate
} // osl

// not KNIGHT
template<osl::Player P>
bool osl::checkmate::ImmediateCheckmate::
hasCheckmateDrop(EffectState const& state, Square target,
		 King8Info canMoveMask,Move& win_move)
{
  mask_t dropPtypeMask=mask_t(Immediate_Checkmate_Table.dropPtypeMask(canMoveMask));
  while(dropPtypeMask){
    Ptype ptype{take_one_bit(dropPtypeMask)+Ptype_Basic_MIN};
    if(state.hasPieceOnStand(P,ptype) &&
       detail::slowCheckDrop<P>(state,target,ptype,canMoveMask,
					    win_move))
      return true;
  }
  return false;
}

template<osl::Player P>
bool osl::checkmate::ImmediateCheckmate::
slowHasCheckmateMoveDirPiece(EffectState const& state, Square target,
			     King8Info canMoveMask,Direction d,Square pos,Piece p,Ptype ptype,Move& win_move){
  const Player altP=alt(P);
  // ptypeがPROOKの時は，更なるチェックが必要
  if(ptype==PROOK){
    int dx=target.x()-pos.x();
    int dy=target.y()-pos.y();
    if(abs(dx)==1 && abs(dy)==1){
      {
	Square pos1=pos+make_offset(dx,0);
	Piece p1=state.pieceAt(pos1);
	if(!p1.isEmpty()){
	  {
	    //  * -OU *
	    // (A)(B)(D)
	    //  * (C) *
	    // (E) *  *
	    // +RY (C) -> (A), (E) -> (A)
	    // -?? - (B)
	    // (D) - 竜以外の利きなし 
	    Square pos2=pos+make_offset(2*dx,0);
	    if(state.pieceAt(pos2).canMoveOn<altP>()){
	      EffectPieceMask effect2=state.effectAt(pos2);
	      if(effect2.countEffect(P)==0 ||
		 (effect2.countEffect(P)==1 &&
		  effect2.test(p.id())))
		return false;
	    }
	  }
	  {
	    //  * -OU *
	    // (A)(B) *
	    //  * (C) *
	    // +RY (C) -> (A)
	    // -?? - (B)竜でpinされているが実はAへの利き持つ
	    if(p.square()==target-make_offset(0,2*dy)
               && state.hasEffectByPiece(p1,pos))
	      return false;
	  }
	}
      }
      {
	Square pos1=pos+make_offset(0,dy);
	Piece p1=state.pieceAt(pos1);
	if(!p1.isEmpty()){
	  Square pos2=pos+make_offset(0,2*dy);
	  {
	    if(state.pieceAt(pos2).canMoveOn<altP>()){
	      EffectPieceMask effect2=state.effectAt(pos2);
	      if(effect2.countEffect(P)==0 ||
		 (effect2.countEffect(P)==1 &&
		  effect2.test(p.id())))
		return false;

	    }
	    {
	      // (C)(B)-OU
	      //  * (A) *
	      // +RY (C) -> (A)
	      // -?? - (B)竜でpinされているが実はAへの利き持つ
	      if(p.square()==target-make_offset(2*dx,0)
                 && state.hasEffectByPiece(p1,pos))
		return false;
	    }
	  }
	}
      }
    }
  }
  // 元々2つの利きがあったマスが，
  // block & 自分の利きの除去で利きがなくなることはあるか?
  // -> ある．
  // +KA  *   *
  //  *  (A) +KI
  //  *  -OU (B)
  //  *   *   *
  // で金がAに移動して王手をかけると，Bの利きが2から0になる．
  mask_t mask=mask_t((canMoveMask>>16)&Immediate_Checkmate_Table.noEffectMask(ptype,d));
  if(mask){
    int num=p.id();
    EffectPieceMask effect2=state.effectAt(pos);
    effect2.reset(num+8);
    mask_t longEffect2=effect2.selectLong();
    longEffect2&=(state.piecesOnBoard(P).to_ullong()<<8);
    do {
      Direction d1{take_one_bit(mask)};
      Square pos1=target-to_offset<P>(d1);
      EffectPieceMask effect1=state.effectAt(pos1);
      int count=effect1.countEffect(P);
      // 自分の利きの除去
      if(effect1.test(num)) count--;
      if(count==0) return false;
      // blockしている利きの除去
      for (int num1: long_to_piece_id_range(effect1.to_ullong()&longEffect2)) {
	if(is_between_unsafe(pos,state.pieceOf(num1).square(),pos1))
	  count--;
	if(count==0) return false;
      }
    } while (mask);
  }
  // 自殺手でないことのチェックを入れる
  if(move_classifier::KingOpenMove::isMember<P>(state,ptype,p.square(),pos)) return false;
  {
    win_move=Move(p.square(),pos,ptype,
		  state.pieceAt(pos).ptype(),
		  ptype!=p.ptype(),P);
  }
  return true;
}

template<osl::Player P>
bool osl::checkmate::ImmediateCheckmate::
hasCheckmateMoveDirPiece(EffectState const& state, Square target,
			 King8Info canMoveMask,Direction d,Square pos,Piece p,Move& win_move){
  Square from=p.square();
  Ptype ptype=p.ptype();
  // 相手の利きが伸びてしまって移動後も利きがついてくる可能性
  {
    const Player altP=alt(P);
    Direction d1=base8_dir_unsafe<P>(from,pos);
    if(Int(d1)!=Direction_INVALID_VALUE){ // not knight move
      int num=state.ppLongState()[p.id()][P==BLACK ? d1 : inverse(d1)];
      if(num != Piece_ID_EMPTY && state.pieceOf(num).isOnBoardByOwner<altP>())
	return false;
    }
  }
  if(can_promote(ptype) &&
     (from.isPromoteArea(P) || pos.isPromoteArea(P))){
    Ptype pptype=promote(ptype);
    if((((canMoveMask>>8)|0x100)&
	Immediate_Checkmate_Table.noEffectMask(pptype,d))==0){
      if(slowHasCheckmateMoveDirPiece<P>(state,target,canMoveMask,d,pos,p,pptype,win_move)) return true;
    }
    if (ptype==PAWN || /*basic because canpromote*/is_major_basic(ptype)) 
      return false;
  }
  if((((canMoveMask>>8)|0x100)&
      Immediate_Checkmate_Table.noEffectMask(ptype,d))==0){
    if(slowHasCheckmateMoveDirPiece<P>(state,target,canMoveMask,d,pos,p,ptype,win_move)) return true;
  }
  return false;
}

template<osl::Player P>
bool osl::checkmate::ImmediateCheckmate::
hasCheckmateMoveDir(EffectState const& state, Square target,
		    King8Info canMoveMask,Direction d,Move& win_move){
  Square pos=target-to_offset<P>(d);
  if(state.countEffect(P,pos)<2 &&
     ! AdditionalEffect::hasEffect(state,pos,P)) return false;
  PieceMask pieceMask=state.piecesOnBoard(P)&state.effectAt(pos);
  assert(pos.isOnBoard());
  // 玉で王手をかけない
  pieceMask.reset(king_piece_id(P));
  for (int num: pieceMask.toRange()) {
    if(hasCheckmateMoveDirPiece<P>(state,target,canMoveMask,d,pos,state.pieceOf(num),win_move))
      return true;
  }
  return false;
}

// not KNIGHT
template<osl::Player P>
bool osl::checkmate::ImmediateCheckmate::
hasCheckmateMove(EffectState const& state, Square target,
		 King8Info canMoveMask,Move& win_move)
{
  assert(! state.inCheck());
  mask_t mask2 = moveCandidate2(canMoveMask);
  while(mask2){
    Direction d{take_one_bit(mask2)};
    if(hasCheckmateMoveDir<P>(state,target,canMoveMask,d,win_move)) return true;
  }
  return false;
}

template<osl::Player P>
bool osl::checkmate::ImmediateCheckmate::
hasCheckmateMove(EffectState const& state, King8Info canMoveMask,
		 Square target, Move& win_move)
{
  assert(! state.inCheck());
  assert(target.isOnBoard());

  if(hasCheckmateMove<P>(state,target,canMoveMask,win_move)) return true;
  if(detail::hasCheckmateMoveKnight<P>(state,target,canMoveMask,win_move)) return true;
  return hasCheckmateDrop<P>(state,target,canMoveMask,win_move);
}

template<osl::Player P>
bool osl::checkmate::ImmediateCheckmate::
hasCheckmateMove(EffectState const& state,Move& win_move)
{
  const Player altP=alt(P);
  const Square target=state.kingSquare(altP);
  return hasCheckmateMove<P>(state, state.king8Info(altP), target, win_move);
}

namespace osl
{
  namespace checkmate
  {
    template 
    bool ImmediateCheckmate::
    hasCheckmateMove<BLACK>(EffectState const&, King8Info, Square, Move&);
    template 
    bool osl::checkmate::ImmediateCheckmate::
    hasCheckmateMove<WHITE>(EffectState const&, King8Info, Square, Move&);

    template 
    bool ImmediateCheckmate::
    hasCheckmateMove<BLACK>(EffectState const&, Move&);
    template 
    bool osl::checkmate::ImmediateCheckmate::
    hasCheckmateMove<WHITE>(EffectState const&, Move&);
  }
}

bool osl::checkmate::ImmediateCheckmate::
hasCheckmateMove(Player pl,EffectState const& state)
{
  Move win_move;
  return hasCheckmateMove(pl, state, win_move);
}
bool osl::checkmate::ImmediateCheckmate::
hasCheckmateMove(Player pl,EffectState const& state,Move& win_move)
{
  if(pl==BLACK)
    return hasCheckmateMove<BLACK>(state,win_move);
  else
    return hasCheckmateMove<WHITE>(state,win_move);
}

// additionalEffect.cc
bool osl::
AdditionalEffect::hasEffect(const EffectState& state, Square target, 
			       Player attack)
{
  PieceMask direct = state.effectAt(target) & state.piecesOnBoard(attack);
  PieceMask mask;
  mask.setAll();
  mask.clearBit<KNIGHT>();
  direct &= (state.promotedPieces() | mask);

  for (int num: direct.toRange()) {
    const Square p = state.pieceOf(num).square();
    const Direction d=base8_dir<BLACK>(p,target);
    const int num1=state.ppLongState()[num][d];
    if(!Piece::isEmptyNum(num1) && state.pieceOf(num1).owner()==attack)
      return true;
  }
  return false;
}

// addEffectWithEffect
namespace osl
{
  namespace move_generator
  {
    namespace detail
    {
      typedef Store Action;
      /**
       * マスtoに移動可能な駒pを移動する手を生成する．
       * ptypeMaskで指定されたptypeになる場合以外は手を生成しない．
       * @param state - 盤面
       * @param p - 利きを持つコマ
       * @param to - 目的のマス
       * @param toP - 目的のマスに現在ある駒(又は空白)
       * @param action - 手生成のaction(典型的にはstoreかfilterつきstore)
       * @param ptypeMask - 移動後の駒のptypeに対応するbitが1なら手を生成する
       * should promoteは?
       * 呼び出す時はpinnedの場合のunsafeなdirectionは排除済み
       */
      template<Player P>
      void generateMovePiecePtypeMask(const EffectState& state,Piece p,Square to,Piece toP,Action& action,unsigned int ptypeMask)
      {
	assert(p.isOnBoardByOwner<P>());
	assert(toP==state.pieceAt(to));
	Ptype ptype=p.ptype();
	Square from=p.square();
	if(can_promote(ptype) &&
	   (to.isPromoteArea(P) || from.isPromoteArea(P))){
	  Ptype pptype=osl::promote(ptype);
	  if (bittest(ptypeMask, pptype))
	    action.unknownMove(from,to,toP,pptype,true,P);
	  if(Move::ignoreUnpromote(P,ptype,from,to)) return;
	}
	// 
	if(bittest(ptypeMask, ptype))
	  action.unknownMove(p.square(),to,toP,ptype,false,P);
      }
      /**
       * あるマスに利きを持つすべての駒の中で，
       * ptypeMaskで指定されたptypeになる場合は移動する手を生成する
       * @param state - 盤面
       * @param to - 目的のマス
       * @param toP - 目的のマスに現在ある駒(又は空白)
       * @param action - 手生成のaction(典型的にはstoreかfilterつきstore)
       * @param ptypeMask - 移動後の駒のptypeに対応するbitが1なら手を生成する
       * pinnedの場合は移動する手が1手もない場合もある．
       */
      template<Player P>
      void generateMoveToPtypeMaskWithPieceMask(const EffectState& state,Square to,Piece toP,Action& action,unsigned int ptypeMask,PieceMask pieceMask)
      {
	assert(! pieceMask.test(king_piece_id(P)));
	for (int num: pieceMask.toRange()){
	  Piece p=state.pieceOf(num);
	  if(state.pinOrOpen(P).test(num)){
	    Direction d=state.pinnedDir<P>(p);
	    Direction d1=base8_dir_unsafe<P>(p.square(),to); // both are onboard
	    if(primary(d)!=primary(d1)) continue;
	  }
	  generateMovePiecePtypeMask<P>(state,p,to,toP,action,ptypeMask);
	}
      }
      template<Player P>
      void generateMoveToPtypeMask(const EffectState& state,Square to,Piece toP,Action& action,unsigned int ptypeMask)
      {
	PieceMask pieceMask=state.piecesOnBoard(P)&state.effectAt(to);
	pieceMask.reset(king_piece_id(P)); // 玉は除く
	pieceMask &= ~state.pinOrOpen(alt(P)); // open atackからのものを除く
	generateMoveToPtypeMaskWithPieceMask<P>(state,to,toP,action,ptypeMask,pieceMask);
      }

      /**
       * 敵玉の前に歩を置いた場合に遮った利きで敵玉にlibertyが生まれるかどうか?
       */
      template<Player P>
      bool blockingU(const EffectState& state,Square pos)
      {
	const Player altP=alt(P);
	auto effect=state.effectAt(pos);
	mask_t mask=(effect.to_ullong()& EffectPieceMask::long_mask());
	mask&=state.piecesOnBoard(P).to_ullong()<<8; // ROOK, BISHOPの利きのみのはず
        for (int num: long_to_piece_id_range(mask)) {
	  Square from=state.pieceOf(num).square();
	  if( (P==BLACK ? from.y()>=pos.y() : pos.y()>=from.y()) ){
	    Square shadowPos=pos+base8_step(to_offset32(pos,from));
	    assert((P==BLACK ? shadowPos.y()<=pos.y() : pos.y()<=shadowPos.y()) );
	    Piece p=state.pieceAt(shadowPos);
	    if(p.canMoveOn<altP>() && state.countEffect(P,shadowPos) <= 1){
	      return true;
	    }
	  }
	}
	return false;
      }

      template <Direction d>
      constexpr int direction_type() {
        static_assert(is_base8(d));
        if constexpr (d == U) return 0;
        if constexpr (d == L || d == R || d == D) return 1;
        return 2; // UL, UR, DL, DR
      }
      /**
       * int DirType : 0  - U
       *               1  - LRD
       *               2  - UL,UR,DL,DR
       * dirOffset = to_offset<P>(Dir)
       */
      template<Player P,int DirType>
      void generateDir(const EffectState& state,Square king,Action& action,bool& hpc,
                       Offset dirOffset,Direction Dir,Direction primary,int ptypeMaskNotKing)
      {
	const Player altP=alt(P);
	Square king_neighbor=king-dirOffset;
	if(!king_neighbor.isOnBoard()) return;
	Piece neighbor_piece=state.pieceAt(king_neighbor);
	if(neighbor_piece.isOnBoardByOwner<P>()){
	  if constexpr (DirType==0)
            if (state.hasLongEffectAt<LANCE>(P,king_neighbor))
              PieceOnBoard<>::generate<P,true>(state,neighbor_piece,action,one_hot(primary));
	  return;
	}
	if((state.king8Info(altP)&one_hot(40+Int(Dir)))!=0){
	  // - king_neighborに攻め方の利きがある
	  // TODO safe moveではない
          // ptypeMaskNotKing で 「そこに行けば王手になるコマ」が与えられている
	  generateMoveToPtypeMask<P>(state,king_neighbor,neighbor_piece,action,ptypeMaskNotKing);
	}
	if constexpr (DirType !=0) return;
	if(! neighbor_piece.isEmpty())
          return;
        Square far_sq=state.kingVisibilityOfPlayer(altP,Dir);
        mask_t lance_mask=state.longEffectAt<LANCE>(far_sq,P);
        if(lance_mask){
          Piece far_p=state.pieceAt(far_sq);
          if(far_p.isOnBoardByOwner<P>()){
            PieceOnBoard<>::generate<P,true>(state,far_p,action,one_hot(primary));
            // 
            if(state.hasEffectByPiece(far_p,king_neighbor)){
              // pawn supported by lance
              PieceOnBoard<>::generatePiece<P>(state,far_p,king_neighbor,Piece::EMPTY(),action);
            }
          }
          else if(far_p.isOnBoardByOwner<altP>()){
            assert(! has_multiple_bit(lance_mask));
            int num=take_one_bit(lance_mask);
            Piece p2=state.pieceOf(num);
            if(!state.pinOrOpen(P).test(num) || state.kingSquare<P>().isUD(p2.square())){
              action.unknownMove(p2.square(),far_sq,far_p,LANCE,false,P);
            }
          }
        }
        // - PAWN, LANCEはここで調べる?
        //  + ただしPAWNはつみは禁止
        if(! state.pawnInFile<P>(king.x()) && state.hasPieceOnStand<PAWN>(P)){
          // 利きをさえぎるパターンの検証
          if(((state.king8Info(altP)&(0xff00ull|one_hot(Int(U)+24)))^one_hot(Int(U)+24))!=0
             || blockingU<P>(state,king_neighbor))
            action.dropMove(king_neighbor,PAWN,P);
          else
            hpc=true; // has pawn checkmate (illegal)
        }
        if(state.hasPieceOnStand<LANCE>(P)){
          action.dropMove(king_neighbor,LANCE,P);
          auto step = to_offset(P,U);
          auto move = Move(king_neighbor-step, LANCE, P);
          for(auto to=king_neighbor-step; to!=far_sq; to-=step, move=adjust_to(move, -step))
            action(to, move);
        }
      }

      template<Player P,Direction Dir>
      void generateDir(const EffectState& state,Square target,Action& action,bool& hpc)
      {
	generateDir<P,direction_type<Dir>()>(state,target,action,hpc,
                                           to_offset<P>(Dir),Dir,primary(Dir),
                                           ptype_set<Dir>() & ~one_hot(KING));
      }
      template<Player P,bool mustCareSilver>
      void generateOpenOrCapture(const EffectState& state,Square target,Piece p,int num,Action& action)
      {
        // p,num = attack 
	// TODO: pin, captureを作る
	Direction d=base8_dir<P>(p.square(),target);
	Square mid=state.pieceReach((P==BLACK ? d : inverse(d)),num);
	assert(mid.isOnBoard());
	const Player altP=alt(P);
	Square mid1=state.kingVisibilityOfPlayer(altP,d);
	if(mid==mid1){
	  Piece p1=state.pieceAt(mid);
	  assert(p1.isPiece());
	  Square target_next=target+base8_step(p.square(),target); // warning: care sign
	  if((P==BLACK ? p1.pieceIsBlack() : !p1.pieceIsBlack())){
	    // open attack
	    PieceOnBoard<>::generate<P,true>(state,p1,action,one_hot(primary(d)));
	    // p1がtarget_nextに利きを持つ
	    if(state.hasEffectByPiece(p1,target_next)){
	      // silverが斜め下に利きを持つ場合は「成らず」しか生成しない
	      if(mustCareSilver && p1.ptype()==SILVER && 
		 (P==BLACK ? target.y()>mid.y() : target.y()<mid.y())){
		// pinの場合は動ける可能性はない 
		if(!state.pinOrOpen(P).test(p1.id())){
		  action.unknownMove(mid,target_next,Piece::EMPTY(),SILVER,false,P);
		}
	      }
	      else
		PieceOnBoard<>::generatePiece<P>(state,p1,target_next,Piece::EMPTY(),action);
	    }
	  }
	  else{
	    // 隣の場合はすでに作っている
	    if(mid==target_next)
	      return;
	    PieceOnBoard<>::generatePiece<P>(state,p,mid,p1,action);
	  }
	}
      }
      template <Player P, Ptype T>
      inline void try_move_to(const EffectState& state, Piece p, int to_x, int to_y, Action& action) {
        Square to(to_x, to_y);
        Piece p_at = state.pieceAt(to);
        if (p_at.canMoveOn<P>())
          PieceOnBoard<>::generatePiecePtype<P,T>(state, p, to, p_at, action);
      }
      template <Player P,Direction inf, Direction sup>
      inline bool good_mid_sq(const EffectState& state, Square mid, Piece p_mid, int num) {
        return state.effectAt(mid).test(num) && p_mid.canMoveOn<P>()
          && state.kingVisibilityBlackView(alt(P),inf).uintValue() >= mid.uintValue()
          && mid.uintValue() >= state.kingVisibilityBlackView(alt(P),sup).uintValue();
      }
      template<osl::Player P>
      void generateRookLongMove(const EffectState& state,Square target,Action& action)
      {
	const Player altP=alt(P);
	for(int num: to_range(ROOK)) {
	  // pinの場合はすでに作っている
	  if(state.pinOrOpen(altP).test(num)) continue;
	  Piece p=state.pieceOf(num);
	  if(!p.isOnBoardByOwner<P>()) continue;
	  if(target.isULRD(p.square())){
	    generateOpenOrCapture<P,false>(state,target,p,num,action);
	    continue;
	  }
	  int target_x=target.x(),   target_y=target.y();
	  int rook_x=p.square().x(), rook_y=p.square().y();
	  if(p.isPromoted()){
	    if((unsigned int)(target_x-rook_x+1)>2u){ // abs(target_x-rook_x)>1
	      if((unsigned int)(target_y-rook_y+1)>2u){ // abs(target_y-rook_y)>1
		{
		  Square pos(rook_x,target_y);
		  Piece p1=state.pieceAt(pos);
		  if(good_mid_sq<P,R,L>(state, pos, p1, num)
                     && (!state.pinOrOpen(P).test(num) || p.square().isUD(state.kingSquare<P>()))){
		    action.unknownMove(p.square(),pos,p1,PROOK,false,P);
		  }
		}
		{
		  Square pos(target_x,rook_y);
		  Piece p1=state.pieceAt(pos);
		  if(good_mid_sq<P,U,D>(state, pos, p1, num)
                     && (!state.pinOrOpen(P).test(num) || p.square().isLR(state.kingSquare<P>()))){
		    action.unknownMove(p.square(),pos,p1,PROOK,false,P);
		  }
		}
	      }
	      else{ // (abs(target_x-rook_x)>1 && abs(target_y-rook_y)==1
		int min_x=state.kingVisibilityBlackView(altP,L).x();
		int max_x=state.kingVisibilityBlackView(altP,R).x();
		if(target_x>rook_x) max_x=target_x-2;
		else min_x=target_x+2;
		min_x=std::max(min_x,rook_x-1);
		max_x=std::min(max_x,rook_x+1);
		for (int x=min_x;x<=max_x;x++)
                  try_move_to<P,PROOK>(state, p, x, target_y, action);
	      }
	    }
	    else if((unsigned int)(target_y-rook_y+1)>2u){ // abs(target_y-rook_y)>1, abs(target_x-rook_x)==1
	      int min_y=state.kingVisibilityBlackView(altP,D).y();
	      int max_y=state.kingVisibilityBlackView(altP,U).y();
	      if(target_y>rook_y) max_y=target_y-2;
	      else min_y=target_y+2;
	      min_y=std::max(min_y,rook_y-1);
	      max_y=std::min(max_y,rook_y+1);
	      for(int y=min_y;y<=max_y;y++)
                try_move_to<P,PROOK>(state, p, target_x, y, action);
	    }
	  }
	  else{ // ROOK
	    // vertical move
	    if((unsigned int)(target_x-rook_x+1)>2u){ // abs(target_x-rook_x)>1
	      Square pos(rook_x,target_y);
	      Piece p1=state.pieceAt(pos);
	      if(good_mid_sq<P,R,L>(state, pos, p1, num)
                 && (!state.pinOrOpen(P).test(num) || p.square().isUD(state.kingSquare<P>()))){
		if(promote_area_y(P, rook_y) || promote_area_y(P, target_y)){
		  action.unknownMove(p.square(),pos,p1,PROOK,true,P);
		}
		else action.unknownMove(p.square(),pos,p1,ROOK,false,P);
	      }
	    }
	    // horizontal move
	    if((unsigned int)(target_y-rook_y+1)>2u){ // abs(target_y-rook_y)>1
	      Square pos(target_x,rook_y);
	      Piece p1=state.pieceAt(pos);
	      if(good_mid_sq<P,U,D>(state, pos, p1, num)
                 && (!state.pinOrOpen(P).test(num) || p.square().isLR(state.kingSquare<P>()))){
		if(promote_area_y(P, rook_y)){
		  action.unknownMove(p.square(),pos,p1,PROOK,true,P);
		}
		else
		  action.unknownMove(p.square(),pos,p1,ROOK,false,P);
	      }
	    }
	  }
	}
      }
      // p,num = bishop
      template<Player P,Ptype T>
      void generateBishopLongMove(const EffectState& state,Square target,Action& action,Piece p,int num)
      {
	const Player altP=alt(P);
	int target_x=target.x(),          target_y=target.y();
	int target_xPy=target_x+target_y, target_xMy=target_x-target_y;
	int bishop_x=p.square().x(),      bishop_y=p.square().y();
	int bishop_xPy=bishop_x+bishop_y, bishop_xMy=bishop_x-bishop_y;
	if(((target_xPy^bishop_xPy)&1)!=0){
	  if constexpr (T==BISHOP) return;
	  // 市松模様のparityの違う場合も，隣ならOK?
	  if((unsigned int)(target_xPy-bishop_xPy+1)<=2u){ // abs(target_xPy-bishop_xPy)==1
	    Square ul=state.kingVisibilityBlackView(altP,UL);
	    Square dr=state.kingVisibilityBlackView(altP,DR);
	    int min_xMy=ul.x()-ul.y(), max_xMy=dr.x()-dr.y();
	    if(target_xMy>bishop_xMy) max_xMy=target_xMy-4;
	    else min_xMy=target_xMy+4;
	    min_xMy=std::max(min_xMy,bishop_xMy-1);
	    max_xMy=std::min(max_xMy,bishop_xMy+1);
	    for (int xMy=min_xMy;xMy<=max_xMy;xMy+=2)
              try_move_to<P,T>(state, p, (target_xPy+xMy)/2, (target_xPy-xMy)/2, action);
	  }
	  else if((unsigned int)(target_xMy-bishop_xMy+1)<=2u){ // abs(target_xMy-bishop_xMy)==1
	    Square dl=state.kingVisibilityBlackView(altP,DL);
	    Square ur=state.kingVisibilityBlackView(altP,UR);
	    int min_xPy=dl.x()+dl.y(), max_xPy=ur.x()+ur.y();
	    if(target_xPy>bishop_xPy) max_xPy=target_xPy-4;
	    else min_xPy=target_xPy+4;
	    min_xPy=std::max(min_xPy,bishop_xPy-1);
	    max_xPy=std::min(max_xPy,bishop_xPy+1);
	    for(int xPy=min_xPy;xPy<=max_xPy;xPy+=2)
              try_move_to<P,T>(state, p, (xPy+target_xMy)/2, (xPy-target_xMy)/2, action);
	  }
	  return;
	}
	//  / 方向(dx==dy)から王手をかける
	if((unsigned int)(target_xPy-bishop_xPy+2)>4u){ // abs(target_xPy-bishop_xPy)>2
	  int pos_x=(bishop_xPy+target_xMy)>>1, pos_y=(bishop_xPy-target_xMy)>>1;
	  Square pos(pos_x,pos_y);
	  if(pos.isOnBoard()){
	    Piece p1=state.pieceAt(pos);
	    if(good_mid_sq<P,UR,DL>(state, pos, p1, num)){
	      PieceOnBoard<>::generatePiecePtype<P,T>(state,p,pos,p1,action);
	    }
	  }
	}
	else if(target_xPy==bishop_xPy){
	  generateOpenOrCapture<P,true>(state,target,p,num,action);
	  return;
	}
	//  \ 方向(dx== -dy)から王手をかける
	if((unsigned int)(target_xMy-bishop_xMy+2)>4u){ // abs(target_xMy-bishop_xMy)>2
	  int pos_x=(target_xPy+bishop_xMy)>>1, pos_y=(target_xPy-bishop_xMy)>>1;
	  Square pos(pos_x,pos_y);
	  if(pos.isOnBoard()){
	    Piece p1=state.pieceAt(pos);
	    if(good_mid_sq<P,DR,UL>(state, pos, p1, num)){
	      PieceOnBoard<>::generatePiecePtype<P,T>(state,p,pos,p1,action);
	    }
	  }
	}
	else if(target_xMy==bishop_xMy){
	  generateOpenOrCapture<P,true>(state,target,p,num,action);
	  return;
	}
      }
    } // namespace detail
    template<Player P, Ptype PTYPE>
    void check_by_drop_gs(const EffectState& state,Square king,MoveStore& act,int spaces) {
      static_assert(PTYPE == GOLD || PTYPE == SILVER);
      if (!state.hasPieceOnStand<PTYPE>(P)) return;
      constexpr auto cm = ptype_move_direction[idx(PTYPE)];
      auto m = spaces & cm;
      if (m==0) return;
      auto s = drop_skelton(PTYPE,P);
      Square to;
      if constexpr(bittest(cm, UL)) if (bittest(m,UL)) to=king-to_offset<P>(UL),act(to,set_skelton_to(s,to));
      if constexpr(bittest(cm, U )) if (bittest(m,U )) to=king-to_offset<P>(U ),act(to,set_skelton_to(s,to));
      if constexpr(bittest(cm, UR)) if (bittest(m,UR)) to=king-to_offset<P>(UR),act(to,set_skelton_to(s,to));
      if constexpr(bittest(cm, L )) if (bittest(m,L )) to=king-to_offset<P>(L ),act(to,set_skelton_to(s,to));
      if constexpr(bittest(cm, R )) if (bittest(m,R )) to=king-to_offset<P>(R ),act(to,set_skelton_to(s,to));
      if constexpr(bittest(cm, DL)) if (bittest(m,DL)) to=king-to_offset<P>(DL),act(to,set_skelton_to(s,to));
      if constexpr(bittest(cm, D )) if (bittest(m,D )) to=king-to_offset<P>(D ),act(to,set_skelton_to(s,to));
      if constexpr(bittest(cm, DR)) if (bittest(m,DR)) to=king-to_offset<P>(DR),act(to,set_skelton_to(s,to));
    }
    
    template <Player P, Ptype PTYPE>
    void check_by_drop_long(const EffectState& state,Square king,MoveStore& action) {
      static_assert(PTYPE == BISHOP || PTYPE == ROOK);
      if (!state.hasPieceOnStand<PTYPE>(P))
        return;
      constexpr auto dircetions = ptype_move_direction[idx(PTYPE)];
      for (int ld: to_range(dircetions)) {
        auto d = long_to_base8(Direction(ld));
        auto limit = state.kingVisibilityOfPlayer(alt(P), d);
        auto step = to_offset(P,d);
        auto move = Move(limit+step, PTYPE, P);
        for (auto to=limit+step; to!=king; to+=step, move=adjust_to(move,step))
          action(to, move);
      }
    }
    template <Player P>
    void check_by_knight(const EffectState& state,Square target,MoveStore& action) {
      const auto dst = {target-to_offset<P>(UUL),target-to_offset<P>(UUR)};
      bool has_knight = state.hasPieceOnStand<KNIGHT>(P);
      for (auto pos: dst) {
	if(!pos.isOnBoard()) continue;
	Piece p=state.pieceAt(pos);
	if(!p.canMoveOn<P>()) continue;
	mask_t mask=state.ptypeEffectAt<KNIGHT>(P, pos);
	mask &= ~state.promotedPieces().to_ullong();
	// pinnedなknightは動けない
	mask &= ~state.pinOrOpen(P).to_ullong();
	for (int num: to_range(mask)) {
	  Piece p1=state.pieceOf(num);
	  action.unknownMove(p1.square(),pos,p,KNIGHT,false,P);
	}
        if (has_knight && p.isEmpty()) 
	  action.dropMove(pos,KNIGHT,P);
      }
    }
  } // namespace move_generator
} // namespace osl

template <osl::Player P>
void osl::move_generator::AddEffect::
generate(const EffectState& state,Square target,Action& action,bool &hpc)
{
  using namespace detail;
  
  const Player altP=alt(P);
  assert(target==state.kingSquare(altP));
  generateDir<P,U>(state,target,action,hpc);
  check_by_knight<P>(state,target,action);
  generateDir<P,UL>(state,target,action,hpc);
  generateDir<P,UR>(state,target,action,hpc);
  generateDir<P,L>(state,target,action,hpc);
  generateDir<P,R>(state,target,action,hpc);
  generateDir<P,D>(state,target,action,hpc);
  generateDir<P,DL>(state,target,action,hpc);
  generateDir<P,DR>(state,target,action,hpc);
  generateRookLongMove<P>(state,target,action);
  for(int num: to_range(BISHOP)){
    // pinの場合はすでに作っている
    if(state.pinOrOpen(altP).test(num)) continue;
    Piece p=state.pieceOf(num);
    if(!p.isOnBoardByOwner<P>()) continue;
    if(p.isPromoted())
      generateBishopLongMove<P,PBISHOP>(state,target,action,p,num);
    else
      generateBishopLongMove<P,BISHOP>(state,target,action,p,num);
  }
  int spaces=osl::checkmate::spaces(state.king8Info(altP));
  check_by_drop_gs<P,GOLD>(state,target,action,spaces);
  check_by_drop_gs<P,SILVER>(state,target,action,spaces);
  // generateDropGold<P>(state,target,action,spaces);
  // generateDropSilver<P>(state,target,action,spaces);

  check_by_drop_long<P,BISHOP>(state,target,action);
  check_by_drop_long<P,ROOK>(state,target,action);
}

namespace osl
{
  template void move_generator::AddEffect::generate<BLACK>(const EffectState&,Square,MoveStore&,bool&);
  template void move_generator::AddEffect::generate<WHITE>(const EffectState&,Square,MoveStore&,bool&);
} // namespace osl

bool osl::win_if_declare(const EffectState& state) {
  const auto Turn = state.turn();

  //手番, 持時間は省略
  assert(Turn == state.turn());
  const Square my_king_sq = state.kingSquare(Turn);

  if (my_king_sq.isPieceStand() || state.hasEffectAt(alt(Turn), my_king_sq))
    return false;

  if (! promote_area_y(Turn, my_king_sq.y()))
    return false;
  
  // 敵陣に自分の駒が10枚以上 (自玉を除いて) あるか
  // 駒の点数を勘定する.  (対象: 敵陣の駒 + 持駒)
  // 大駒を5点として, 先手は28点, 後手なら27点必要
  int pieces_in_area = 0;
  int score_in_area = -1; // 自玉の分を予め引いておく

  for (int n: state.piecesOnBoard(Turn).toRange()) {
    auto p = state.pieceOf(n);
    if (p.square().isPromoteArea(Turn)) {
      ++pieces_in_area;
      score_in_area += 1 + 4 * is_major(p.ptype());
    }
  }
  if (pieces_in_area < 11)
    return false;

  int score_stand = 5 * state.countPiecesOnStand(Turn, ROOK) + 5 * state.countPiecesOnStand(Turn, BISHOP)
    + state.countPiecesOnStand(Turn, GOLD)   + state.countPiecesOnStand(Turn, SILVER)
    + state.countPiecesOnStand(Turn, KNIGHT) + state.countPiecesOnStand(Turn, LANCE)
    + state.countPiecesOnStand(Turn, PAWN);

  return score_in_area + score_stand >= 27 + (Turn==BLACK);
}

// ;;; Local Variables:
// ;;; mode:c++
// ;;; c-basic-offset:2
// ;;; End:

