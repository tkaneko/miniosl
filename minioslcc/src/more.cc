#include "more.h"
#include "record.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cassert>
#include <string>
#include <array>

/* ------------------------------------------------------------------------- */

namespace osl 
{
  namespace move_generator
  {
    namespace drop
    {
      // skeltons
      const auto hall = Square::STAND();
      const CArray<Move,2> s_rook = { Move(hall,ROOK,BLACK), Move(hall,ROOK,WHITE) },
        s_bishop = { Move(hall,BISHOP,BLACK), Move(hall,BISHOP,WHITE) },
        s_gold = { Move(hall,GOLD,BLACK), Move(hall,GOLD,WHITE) },
        s_silver = { Move(hall,SILVER,BLACK), Move(hall,SILVER,WHITE) };
      const Move s_none = Move::PASS(BLACK);
      /**
       * Nは有効なptypeの数
       * http://d.hatena.ne.jp/LS3600/200911 2009-11-10 参照
       */
      template<Player P,bool hasPawn,bool hasLance,bool hasKnight,int N>
      void 
      generateX(const EffectState& state,Store& action,int x,Move m1,Move m2,Move m3,
                Ptype t1,Ptype t2,Ptype t3)
      {
	assert(hasPawn || hasLance || hasKnight || N>0);
        constexpr int y1 = change_y_view(P, 1), y2 = change_y_view(P, 2);
        constexpr int y3 = change_y_view(P, 3);
        
        Move m4;
        if constexpr (N==4) {
          m1 = s_rook[P];  m2 = s_bishop[P];
          m3 = s_gold[P];  m4 = s_silver[P];
        }
        if constexpr (N>0) {
          Square pos(x,y1);
          Piece p=state.pieceAt(pos);
          if (p.isEmpty()) {
            if constexpr (N>=1) action(pos, set_skelton_to(m1, pos));
            if constexpr (N>=2) action(pos, set_skelton_to(m2, pos));
            if constexpr (N>=3) action(pos, set_skelton_to(m3, pos));
            if constexpr (N>=4) action(pos, set_skelton_to(m4, pos));
          }
        }
        if constexpr (hasPawn || hasLance || N>0) {
          Square pos(x,y2);
          Piece p=state.pieceAt(pos);
          if (p.isEmpty()) {
            if constexpr (N>=1) action(pos, set_skelton_to(m1, pos));
            if constexpr (N>=2) action(pos, set_skelton_to(m2, pos));
            if constexpr (N>=3) action(pos, set_skelton_to(m3, pos));
            if constexpr (N>=4) action(pos, set_skelton_to(m4, pos));
            if constexpr (hasLance) action(pos, Move(pos,LANCE,P));
            if constexpr (hasPawn)  action(pos, Move(pos,PAWN,P));
          }
        }
        for (int y=y3; change_y_view(P,y)<=9; y+= sign(P)) {
          Square pos(x,y);
          Piece p=state.pieceAt(pos);
          if (p.isEmpty()) {
            if constexpr (N>=1) action(pos, set_skelton_to(m1, pos));
            if constexpr (N>=2) action(pos, set_skelton_to(m2, pos));
            if constexpr (N>=3) action(pos, set_skelton_to(m3, pos));
            if constexpr (N>=4) action(pos, set_skelton_to(m4, pos));
            if constexpr (hasKnight) action(pos, Move(pos,KNIGHT,P));
            if constexpr (hasLance)  action(pos, Move(pos,LANCE,P));
            if constexpr (hasPawn)   action(pos, Move(pos,PAWN,P));
          }
        }
      }

      template<Player P,bool hasPawn,bool hasLance,bool hasKnight,int N>
      void 
      generate(const EffectState& state,Store& action,Move m1,Move m2,Move m3,
               Ptype t1=Ptype_EMPTY,Ptype t2=Ptype_EMPTY,Ptype t3=Ptype_EMPTY)
      {
	if constexpr (hasPawn || hasLance || hasKnight || N>0) {
	  if constexpr (hasPawn){
	    if constexpr (hasLance || hasKnight || N>0) {
	      for(int x=9;x>0;x--){
		if(state.pawnInFile<P>(x))
		  generateX<P,false,hasLance,hasKnight,N>(state,action,x,m1,m2,m3,t1,t2,t3);
		else
		  generateX<P,true,hasLance,hasKnight,N>(state,action,x,m1,m2,m3,t1,t2,t3);
	      }
	    }
	    else{
	      for(int x=9;x>0;x--){
		if(!state.pawnInFile<P>(x))
		  generateX<P,true,hasLance,hasKnight,N>(state,action,x,m1,m2,m3,t1,t2,t3);
	      }
	    }
	  }
	  else{ // pawnなし
	    for(int x=9;x>0;x--){
	      generateX<P,false,hasLance,hasKnight,N>(state,action,x,m1,m2,m3,t1,t2,t3);
	    }
	  }
	}
      }


      template<Player P,bool hasPawn,bool hasLance,bool hasKnight>
      static void checkSilver(const EffectState& state,Store& action)
      {
        const bool has_silver = state.hasPieceOnStand<SILVER>(P);
        const bool has_gold = state.hasPieceOnStand<GOLD>(P);
        const bool has_bishop = state.hasPieceOnStand<BISHOP>(P);
        const bool has_rook = state.hasPieceOnStand<ROOK>(P);
	if(has_silver){
	  if (has_gold) {
	    if (has_bishop) {
	      if (has_rook) 
		generate<P,hasPawn,hasLance,hasKnight,4>(state,action, s_none,s_none,s_none);
	      else
		generate<P,hasPawn,hasLance,hasKnight,3>
                  (state,action, s_bishop[P],s_gold[P],s_silver[P], BISHOP,GOLD,SILVER);
	    }
	    else if (has_rook) 
	      generate<P,hasPawn,hasLance,hasKnight,3>(state,action,
		s_rook[P], s_gold[P], s_silver[P], ROOK,GOLD,SILVER);
	    else
	      generate<P,hasPawn,hasLance,hasKnight,2>
                (state,action, s_gold[P],s_silver[P],s_none, GOLD,SILVER);
	  } // GOLD
	  else if (has_bishop) {
	    if (has_rook) 
	      generate<P,hasPawn,hasLance,hasKnight,3>
                (state,action, s_rook[P],s_bishop[P],s_silver[P], ROOK,BISHOP,SILVER);
	    else
	      generate<P,hasPawn,hasLance,hasKnight,2>
                (state,action, s_bishop[P],s_silver[P],s_none, BISHOP,SILVER);
	    }
	  else if (has_rook) 
	    generate<P,hasPawn,hasLance,hasKnight,2>
              (state,action, s_rook[P],s_silver[P],s_none, ROOK,SILVER);
	  else
	    generate<P,hasPawn,hasLance,hasKnight,1>
              (state,action, s_silver[P],s_none,s_none, SILVER);
	}
	else if (has_gold) {
	  if (has_bishop) {
	    if (has_rook) 
	      generate<P,hasPawn,hasLance,hasKnight,3>
                (state,action, s_rook[P],s_bishop[P],s_gold[P], ROOK,BISHOP,GOLD);
	    else
	      generate<P,hasPawn,hasLance,hasKnight,2>
                (state,action, s_bishop[P],s_gold[P],s_none, BISHOP,GOLD);
	  }
	  else if (has_rook) 
	    generate<P,hasPawn,hasLance,hasKnight,2>
              (state,action, s_rook[P],s_gold[P],s_none, ROOK,GOLD);
	  else
	    generate<P,hasPawn,hasLance,hasKnight,1>
              (state,action, s_gold[P],s_none,s_none, GOLD);
	} 
	else if (has_bishop) {
	  if (has_rook) 
	    generate<P,hasPawn,hasLance,hasKnight,2>
              (state,action, s_rook[P],s_bishop[P],s_none, ROOK,BISHOP);
	  else
	    generate<P,hasPawn,hasLance,hasKnight,1>
              (state,action, s_bishop[P],s_none,s_none, BISHOP);
	}
	else if (has_rook) 
	  generate<P,hasPawn,hasLance,hasKnight,1>
            (state,action, s_rook[P],s_none,s_none, ROOK);
	else
	  generate<P,hasPawn,hasLance,hasKnight,0>
            (state,action, s_none,s_none,s_none);
      }

      template<Player P,bool hasPawn,bool hasLance>
      static void checkKnight(const EffectState& state,Store& action)
      {
	if (state.hasPieceOnStand<KNIGHT>(P))
	  checkSilver<P,hasPawn,hasLance,true>(state,action);
	else
	  checkSilver<P,hasPawn,hasLance,false>(state,action);
      }

      template<Player P,bool hasPawn>
      static void checkLance(const EffectState& state,Store& action) {
	if(state.hasPieceOnStand<LANCE>(P))
	  checkKnight<P,hasPawn,true>(state,action);
	else
	  checkKnight<P,hasPawn,false>(state,action);
      }
    } // namespace drop

    template<osl::Player P>
    void osl::move_generator::Drop::
    generate(const EffectState& state,Store& action) {
      using drop::checkLance;
      if(state.hasPieceOnStand<PAWN>(P))
	checkLance<P,true>(state,action);
      else
	checkLance<P,false>(state,action);
    }
  } // namespace move_generator
} // namespace osl
namespace osl
{
  namespace move_generator
  {
    // explicit template instantiation
    template void Drop::generate<BLACK>(const EffectState&,MoveStore&);
    template void Drop::generate<WHITE>(const EffectState&,MoveStore&);
  } // namespace move_generator
} // namespace osl

namespace osl
{
  namespace move_generator
  {
    /**
     * ROOK, BISHOP, PROOK, PBISHOPのlong方向の手生成
     * CanPはNoPromoteかAssuredPromote, PromoteOnDirectionのみ
     * NoPromoteはpromoteできない点からの後ろ，横のdirection
     * AssuredPromoteはpromoteできる点から
     * PromoteOnDirectionはpromoteできない点からの前向き direction
     */
    template <Player P,PromoteType CanP,Direction Dir,bool PlainOnly>
    void move_piece_long(EffectState const& state,Piece p,const Piece *ptr,Square from,MoveStore& action,Move moveBase,Ptype ptype) {
      const Direction shortDir=long_to_base8(Dir);
      Square limit=state.pieceReach((P==BLACK ? shortDir : inverse(shortDir)), p);
      const Piece *limitPtr=state.getPiecePtr(limit);
      assert(ptype!=LANCE);
      const Offset offset=to_offset(P,Dir);
      assert(offset != Offset_ZERO);
      ptr+=Int(offset);
      Square to=from+offset;
      Move m= adjust_to(moveBase, offset);
      if constexpr (CanP==PromoteOnDirection || CanP==AssuredPromote){
        if constexpr (CanP==PromoteOnDirection){
          // promoteできない数
          int count=(P==BLACK ? from.y1()-5 : 7-from.y1()); 
          for(int i=0;i<count;i++){
            if(ptr==limitPtr){
              Piece p1= *limitPtr;
              if (!PlainOnly && p1.canMoveOn<P>())
                action(to, m.newAddCapture(p1));
              return;
            }
            action(to, m);
            ptr+=Int(offset);
            to+=offset; m=adjust_to(m, offset);
          }
        }
        if(PlainOnly) return;
        while(ptr!=limitPtr){
          assert(from.isPromoteArea(P) || to.isPromoteArea(P));
          action(to, m.promote());
          ptr+=Int(offset);
          to+=offset;
          m = adjust_to(m, offset);
        }
        Piece p1= *limitPtr;
        if(p1.canMoveOn<P>()){
          m=m.newAddCapture(p1);
          assert(from.isPromoteArea(P) || to.isPromoteArea(P));
          action(to, m.promote());
        }
      }
      else{ // NoPromote
        while(ptr!=limitPtr){
          action(to, m);
          ptr+=Int(offset);
          to+=offset; m = adjust_to(m, offset);
        }
        if(PlainOnly) return;
        Piece p1= *limitPtr;
        if(p1.canMoveOn<P>()){
          m=m.newAddCapture(p1);
          action(to, m);
        }
      }
    }

    template <Player P,Ptype T,PromoteType CanP,Direction Dir,bool PlainOnly>
    inline void
    move_piece_long_if(EffectState const& state,Piece p,const Piece *ptr, Square pos,MoveStore& action,Move moveBase,Ptype ptype)
    {
      if constexpr (! bittest(ptype_move_direction[idx(T)], Dir))
        return;
      constexpr auto Type = (CanP != PromoteOnDirection
                             ? CanP : (is_forward(Dir) ? PromoteOnDirection : NoPromote));
      move_piece_long<P,Type,Dir,PlainOnly>(state,p,ptr,pos,action,moveBase,ptype);
    }

    /**
     * 短い利きの動き
     * AssuredPromote - promote可能な動きの時
     * MustPromote - 2段目の歩，3,4段目の桂馬
     */
    template <Player P,PromoteType CanP,Direction Dir,bool PlainOnly>
    void move_piece_short(const Piece *ptr,Square from,MoveStore& action,Move moveBase,Ptype ptype)
    {
      const Offset offset=to_offset(P,Dir);
      Piece p1=ptr[Int(offset)];	
      Square to=from+offset;
      Move m=adjust_to(moveBase, offset).newAddCapture(p1);
      if ((PlainOnly ? p1.isEmpty() : p1.canMoveOn<P>())){
        if constexpr (!PlainOnly && (CanP==AssuredPromote || CanP==MustPromote))
          action(to, m.promote());
        if constexpr (CanP!=MustPromote)
          action(to, m);
      }
    }

    template <Player P,Ptype T,PromoteType CanP,Direction Dir,bool PlainOnly>
    void move_piece_short_if(const Piece *ptr,Square from,MoveStore& action,Move moveBase,Ptype /*ptype*/)
    {
      if constexpr (! bittest(ptype_move_direction[idx(T)], Dir))
        return;
      constexpr auto Type = (CanP != PromoteOnDirection
                             ? CanP : (is_forward(Dir) ? AssuredPromote : NoPromote));
      move_piece_short<P,Type,Dir,PlainOnly>(ptr,from,action,moveBase,T);
    }

    template <Player P,Ptype T,PromoteType CanP,bool useDirMask,bool PlainOnly>
    void move_piece_promote_type(const EffectState& state,Piece p, MoveStore& action,Square from,int dirMask)
    {
      const Ptype ptype=(T==GOLD ? p.ptype() : T);
      Move moveBase=Move(from,from,ptype,(Ptype)0,false,P);
      const Piece *ptr=state.getPiecePtr(from);
      if(!useDirMask || !bittest(dirMask, UL)){
	move_piece_short_if<P,T,CanP,UL,PlainOnly>(ptr,from,action,moveBase,ptype);
	move_piece_short_if<P,T,CanP,DR,PlainOnly>(ptr,from,action,moveBase,ptype);
	move_piece_long_if<P,T,CanP,Long_UL,PlainOnly>(state,p,ptr,from,action,moveBase,ptype);
	move_piece_long_if<P,T,CanP,Long_DR,PlainOnly>(state,p,ptr,from,action,moveBase,ptype);
      }
      if(!useDirMask || !bittest(dirMask, UR)){
	move_piece_short_if<P,T,CanP,UR,PlainOnly>(ptr,from,action,moveBase,ptype);
	move_piece_short_if<P,T,CanP,DL,PlainOnly>(ptr,from,action,moveBase,ptype);
	move_piece_long_if<P,T,CanP,Long_UR,PlainOnly>(state,p,ptr,from,action,moveBase,ptype);
	move_piece_long_if<P,T,CanP,Long_DL,PlainOnly>(state,p,ptr,from,action,moveBase,ptype);
      }
      if(!useDirMask || !bittest(dirMask, U)){
	move_piece_short_if<P,T,CanP,U,PlainOnly>(ptr,from,action,moveBase,ptype);
	move_piece_short_if<P,T,CanP,D,PlainOnly>(ptr,from,action,moveBase,ptype);
	move_piece_long_if<P,T,CanP,Long_U,PlainOnly>(state,p,ptr,from,action,moveBase,ptype);
	move_piece_long_if<P,T,CanP,Long_D,PlainOnly>(state,p,ptr,from,action,moveBase,ptype);
      }
      if(!useDirMask || !bittest(dirMask, L)){
	move_piece_short_if<P,T,CanP,L,PlainOnly>(ptr,from,action,moveBase,ptype);
	move_piece_short_if<P,T,CanP,R,PlainOnly>(ptr,from,action,moveBase,ptype);
	move_piece_long_if<P,T,CanP,Long_L,PlainOnly>(state,p,ptr,from,action,moveBase,ptype);
	move_piece_long_if<P,T,CanP,Long_R,PlainOnly>(state,p,ptr,from,action,moveBase,ptype);
      }
      move_piece_short_if<P,T,CanP,UUL,PlainOnly>(ptr,from,action,moveBase,ptype);
      move_piece_short_if<P,T,CanP,UUR,PlainOnly>(ptr,from,action,moveBase,ptype);
    }
      
    template <Player P,Direction Dir,bool PlainOnly>
    void move_king_1(const Piece *ptr, Square from,MoveStore& action,unsigned int liberty,Move const& moveBase)
    {
      if (! bittest(liberty, Dir))
        return;
      const Offset offset=to_offset(P,Dir);
      Move m=adjust_to(moveBase, offset);
      Square to=from+offset;
      Piece p1=ptr[Int(offset)];
      assert(p1.canMoveOn<P>());
      if(PlainOnly && !p1.isEmpty()) return;
      m=m.newAddCapture(p1);
      action(to, m);
    }

    template <Player P,bool useDirMask,bool PlainOnly>
    void move_king(const EffectState& state, MoveStore& action,Square pos,int dirMask) {
      King8Info king8info(state.king8Info(P));
      unsigned int liberty=osl::checkmate::liberty(king8info);
      Move moveBase(pos,pos,KING,(Ptype)0,false,P);
      const Piece *ptr=state.getPiecePtr(pos);
      if(!useDirMask || !bittest(dirMask, UL)){
        move_king_1<P,UL,PlainOnly>(ptr,pos,action,liberty,moveBase);
        move_king_1<P,DR,PlainOnly>(ptr,pos,action,liberty,moveBase);
      }
      if(!useDirMask || !bittest(dirMask, U)){
        move_king_1<P,U,PlainOnly>(ptr,pos,action,liberty,moveBase);
        move_king_1<P,D,PlainOnly>(ptr,pos,action,liberty,moveBase);
      }
      if(!useDirMask || !bittest(dirMask, UR)){
        move_king_1<P,UR,PlainOnly>(ptr,pos,action,liberty,moveBase);
        move_king_1<P,DL,PlainOnly>(ptr,pos,action,liberty,moveBase);
      }
      if(!useDirMask || !bittest(dirMask, L)){
        move_king_1<P,L,PlainOnly>(ptr,pos,action,liberty,moveBase);
        move_king_1<P,R,PlainOnly>(ptr,pos,action,liberty,moveBase);
      }
    }

    template <Player P,bool useDirMask,bool PlainOnly>
    void move_lance(const EffectState& state, Piece p,MoveStore& action,Square from,int dirMask){
      if (useDirMask && bittest(dirMask, U))
        return;
      const Offset offset=to_offset(P,U);
      Square limit=state.pieceReach((P==BLACK ? U : D), p);
      Square to=limit;
      Piece p1=state.pieceAt(to);
      int limity=(P==BLACK ? to.y() : 10-to.y());
      int fromy=(P==BLACK ? from.y() : 10-from.y());
      int ycount=fromy-limity-1;
      Move m(from,to,LANCE,(Ptype)0,false,P);
      switch(limity){
      case 4: case 5: case 6: case 7: case 8: case 9:{
        if(!PlainOnly && p1.canMoveOn<P>())
          action(to, m.newAddCapture(p1));
        m = adjust_to(m, -offset); to-=offset;
        goto escape4;
      }
      case 3:
        if(!PlainOnly && p1.canMoveOn<P>()){
          Move m1=m.newAddCapture(p1);
          action(to, m1.promote());
          action(to, m1);
        }
        m = adjust_to(m, -offset); to-=offset;
        goto escape4;
      case 2:
        if(!PlainOnly && p1.canMoveOn<P>()){
          Move m1=m.newAddCapture(p1);
          action(to, m1.promote());
        }
        if(fromy==3) return;
        m = adjust_to(m, -offset); to-=offset;
        ycount=fromy-4;
        goto escape2;
      case 0: 
        m= adjust_to(m, -offset); to-=offset;
        if(!PlainOnly)
          action(to, m.promote());
        goto join01;
      case 1: 
        if(!PlainOnly && p1.canMoveOn<P>()){
          action(to, m.newAddCapture(p1).promote());
        }
      join01:
        if(fromy==2) return;
        m= adjust_to(m, -offset); to-=offset;
        if(fromy==3){
          if(!PlainOnly)
            action(to, m.promote());
          return;
        }
        ycount=fromy-4;
        goto escape01;
      default: assert(0);
      }
    escape01:
      if(!PlainOnly)
        action(to, m.promote());
      m= adjust_to(m, -offset); to-=offset;
    escape2:
      if(!PlainOnly)
        action(to, m.promote());
      action(to, m);
      m= adjust_to(m, -offset); to-=offset;
    escape4:
      while(ycount-->0){
        action(to, m);
        m= adjust_to(m, -offset);
        to-=offset;
      }
    }

    template <Player P,bool useDirMask,bool PlainOnly>
    void move_pawn(const EffectState& state, Piece p,MoveStore& action,Square from,int dirMask) {
      assert(from == p.square());
      if (useDirMask && bittest(dirMask, U))
        return;
      if(PlainOnly && (P==BLACK ? from.yLe<4>() : from.yGe<6>())) return;
      Square to = from + to_offset(P,U);
      Piece p1=state.pieceAt(to);
      if(PlainOnly){
        if(p1.isEmpty())
          action(to, Move(from,to,PAWN,Ptype_EMPTY,false,P));
        return;
      }
      if(p1.canMoveOn<P>()){
        if(P==BLACK ? to.yLe<3>() : to.yGe<7>()){ // canPromote
          if(PlainOnly) return;
          Move m(from,to,PPAWN,Ptype_EMPTY,true,P);
          action(to, m.newAddCapture(p1));
        }
        else{
          Move m(from,to,PAWN,Ptype_EMPTY,false,P);
          action(to,m.newAddCapture(p1));
        }
      }
    }
    template <bool PlainOnly>
    template <Player P,Ptype T,bool useDirMask>
    void move_generator::PieceOnBoard<PlainOnly>::
    generatePtypeUnsafe(const EffectState& state,Piece p, MoveStore& action,int dirMask) {
      const Square from=p.square();
      if constexpr (T==KING){
        move_king<P,useDirMask,PlainOnly>(state,action,from,dirMask);
      }
      else if constexpr (T==LANCE){
        move_lance<P,useDirMask,PlainOnly>(state,p,action,from,dirMask);
      }
      else if constexpr (T==PAWN){
        move_pawn<P,useDirMask,PlainOnly>(state,p,action,from,dirMask);
      }
      else if constexpr (can_promote(T)){
        switch (promote_type<T,P>(from)) {
        case MustPromote:
          move_piece_promote_type<P,T,MustPromote,useDirMask,PlainOnly>(state,p,action,from,dirMask); break;
        case AssuredPromote:
          move_piece_promote_type<P,T,AssuredPromote,useDirMask,PlainOnly>(state,p,action,from,dirMask); break;
        case PromoteOnDirection:
          move_piece_promote_type<P,T,PromoteOnDirection,useDirMask,PlainOnly>(state,p,action,from,dirMask); break;
        case NoPromote:
          move_piece_promote_type<P,T,NoPromote,useDirMask,PlainOnly>(state,p,action,from,dirMask); break;
        }
      }
      else
        move_piece_promote_type<P,T,NoPromote,useDirMask,PlainOnly>(state,p,action,from,dirMask);
    }
  
    template <bool PlainOnly>
    template <Player P,Ptype T,bool useDirMask>
    void PieceOnBoard<PlainOnly>::
    generatePtype(const EffectState& state,Piece p, MoveStore& action,int dirMask) {
      int num=p.id();
      if (state.pin(P).test(num)) {
        if constexpr (T==KNIGHT) return;
        Direction d=state.pinnedDir<P>(p);
        generatePtypeUnsafe<P,T,true>(state, p, action, dirMask | (~one_hot(primary(d))));
      }
      else{
        generatePtypeUnsafe<P,T,useDirMask>(state,p,action,dirMask);
      }
    }
    template <bool PlainOnly>
    template <Player P,bool useDirmask>
    void PieceOnBoard<PlainOnly>::
    generate(const EffectState& state,Piece p, MoveStore& action,int dirMask)
    {          
      switch(p.ptype()){
      case PPAWN: case PLANCE: case PKNIGHT: case PSILVER: case GOLD:
        generatePtype<P,GOLD,useDirmask>(state,p,action,dirMask); break;
      case PAWN: 
        generatePtype<P,PAWN,useDirmask>(state,p,action,dirMask); break;
      case LANCE: 
        generatePtype<P,LANCE,useDirmask>(state,p,action,dirMask); break;
      case KNIGHT: 
        generatePtype<P,KNIGHT,useDirmask>(state,p,action,dirMask); break;
      case SILVER: 
        generatePtype<P,SILVER,useDirmask>(state,p,action,dirMask); break;
      case BISHOP: 
        generatePtype<P,BISHOP,useDirmask>(state,p,action,dirMask); break;
      case PBISHOP: 
        generatePtype<P,PBISHOP,useDirmask>(state,p,action,dirMask); break;
      case ROOK: 
        generatePtype<P,ROOK,useDirmask>(state,p,action,dirMask); break;
      case PROOK: 
        generatePtype<P,PROOK,useDirmask>(state,p,action,dirMask); break;
      case KING: 
        generatePtype<P,KING,useDirmask>(state,p,action,dirMask); break;
      default: break;
      }
    }
  } // move_generator
} // namespace osl


namespace osl
{
  namespace move_generator
  {
    // explicit template instantiation
    template void PieceOnBoard<>::generate<BLACK,false>(const EffectState&,Piece,MoveStore&,int);
    template void PieceOnBoard<>::generate<WHITE,false>(const EffectState&,Piece,MoveStore&,int);
    template void PieceOnBoard<>::generatePtype<BLACK,KING,false>(const EffectState&,Piece,MoveStore&,int);
    template void PieceOnBoard<>::generatePtype<WHITE,KING,false>(const EffectState&,Piece,MoveStore&,int);

    template void PieceOnBoard<true>::generate<BLACK,true>(const EffectState&,Piece,MoveStore&,int);
    template void PieceOnBoard<true>::generate<WHITE,true>(const EffectState&,Piece,MoveStore&,int);


    template void PieceOnBoard<false>::generate<BLACK,true>(const EffectState&, Piece, MoveStore&, int);
    template void PieceOnBoard<false>::generate<WHITE,true>(const EffectState&, Piece, MoveStore&, int);
  } // namespace move_generator
} // namespace osl

namespace osl
{
  namespace move_generator
  {
    using namespace effect_action;
    namespace capture
    {
      template<Player P>
      void generate(const EffectState& state,Square target,Store& action,PieceMask pieces) {
	Piece p1=state.pieceAt(target);
	for (int num: pieces.toRange()) {
	  Piece p=state.pieceOf(num);
	  if(state.pinOrOpen(P).test(num) && !state.pinnedCanMoveTo<P>(p,target))
	    continue;
	  PieceOnBoard<>::generatePiece<P>(state,p,target,p1,action);
	}
      }
    }

    template<Player P>
    void Capture::
    generate(const EffectState& state,Square target,Store& action)
    {
      assert(target.isOnBoard());
      PieceMask pieces=state.piecesOnBoard(P)&state.effectAt(target);
      capture::generate<P>(state,target,action,pieces);
    }

    template<Player P>
    void Capture::
    escapeByCapture(const EffectState& state,Square target,Piece piece,Store& action)
    {
      PieceMask pieces=state.piecesOnBoard(P)&state.effectAt(target);
      pieces.reset(piece.id());
      capture::generate<P>(state,target,action,pieces);
    }

  } // namespace move_generator
} // namespace osl

namespace osl
{
  namespace move_generator
  {
    using namespace move_action;
    // explicit template instantiation
    template void Capture::escapeByCapture<BLACK>(const EffectState&, Square, Piece, Store&);
    template void Capture::escapeByCapture<WHITE>(const EffectState&, Square, Piece, Store&);

    template void Capture::generate<BLACK>(EffectState const&, Square, Store&);
    template void Capture::generate<WHITE>(EffectState const&, Square, Store&);
  } // namespace move_generator
} // namespace osl

namespace osl
{
  namespace move_generator
  {
    namespace escape_detail
    {
      /**
       * Tの駒をtoに打つ手を生成する．
       */
      template<Player P,Ptype Type>
      void generateDrop(const EffectState& state,Square to,MoveStore& action) {
	if (! state.hasPieceOnStand<Type>(P)) return;
        if ((Type!=PAWN || !state.pawnInFile(P,to.x()))
            && legal_drop_at(P, Type, to))
          action(to, Move(to,Type,P));
      }
      /*
       * 駒をtoに打つ手を生成する．
       */
      template<Player P>
      void generateDropAll(const EffectState& state,Square to,MoveStore& action) {
	generateDrop<P,PAWN>(state,to,action); 
	generateDrop<P,LANCE>(state,to,action);     
	generateDrop<P,KNIGHT>(state,to,action);    
	generateDrop<P,SILVER>(state,to,action);    
	generateDrop<P,GOLD>(state,to,action);      
	generateDrop<P,BISHOP>(state,to,action);    
	generateDrop<P,ROOK>(state,to,action);      
      }
    } // end of namespace escape_detail
    using escape_detail::generateDropAll;
  } // namespace move_generator
} // namespace osl

template<osl::Player P>
void osl::move_generator::Escape::
generateBlockingKing(const EffectState& state,Piece king,Square attack_from,Store &action) {
  // 短い利きの時にもこちらに入ってしまう
  Square king_sq=king.square();
  Offset step=basic_step(to_offset32(attack_from,king_sq));
  assert(step != Offset_ZERO);
  for(Square to=king_sq+step; to!=attack_from; to+=step) {
    assert(state.pieceAt(to).isEmpty()); 
    Capture::escapeByCapture<P>(state,to,king,action);
    // 駒を置いて
    generateDropAll<P>(state,to,action);
  }
}

template<osl::Player P>
void osl::move_generator::Escape::escape_king(const EffectState& state,Store& action)
{
  const Piece king =state.kingPiece<P>();
  Square king_sq=king.square();
  Piece attacker;
  state.hasEffectAt<alt(P)>(king_sq, attacker);

  // (Type == KING)
  if (attacker==Piece::EMPTY()){
    /** escape only */
    PieceOnBoard<>::generatePtype<P,KING>(state,king,action);
  }
  else {
    auto attack_from=attacker.square();

    Capture::escapeByCapture<P>(state, attack_from, king, action);
    /** escape */
    PieceOnBoard<>::generatePtype<P,KING>(state,king,action);
    /** 合い駒 */
    generateBlockingKing<P>(state,king,attack_from,action);
  }
}

void osl::GenerateEscapeKing::generate(const EffectState& state, MoveVector& out)
{
  const size_t first = out.size();
  {
    MoveStore store(out);
    if (state.turn() == BLACK) {
      move_generator::Escape::escape_king<BLACK>(state, store);
    }
    else {
      move_generator::Escape::escape_king<WHITE>(state, store);
    }
  }
  MoveVector unpromote_moves;
  const size_t last = out.size();
  for (size_t i=first; i<last; ++i) {
    if(out[i].hasIgnoredUnpromote())
      unpromote_moves.push_back(out[i].unpromote());
  }
  out.insert(out.end(), unpromote_moves.begin(), unpromote_moves.end());
}

namespace osl
{
  // explicit template instantiation
  namespace move_generator
  {
    template void Escape::escape_king<BLACK>(const EffectState&, MoveStore&);
    template void Escape::escape_king<WHITE>(const EffectState&, MoveStore&);
    template void Escape::generateBlockingKing<BLACK>(const EffectState&,Piece,Square,MoveStore&);
    template void Escape::generateBlockingKing<WHITE>(const EffectState&,Piece,Square,MoveStore&);
  } // namespace move_generator
} // namespace osl

namespace osl
{
  namespace move_generator
  {
    namespace all_moves
    {
      template<Player P,Ptype T>
      void
      generatePtype(const EffectState& state, Store& action){
	for (int num: to_range(T)) {
	  Piece p=state.pieceOf(num);
	  if(p.isOnBoardByOwner<P>()){
	    if (can_promote(T) && p.isPromoted()){
	      constexpr Ptype PT=ptype_move_type[idx(promote(T))];
	      PieceOnBoard<>::generatePtype<P,PT>(state,p,action);
	    }
	    else{
	      PieceOnBoard<>::generatePtype<P,T>(state,p,action);
	    }
	  }
	}
      }
    }
    using all_moves::generatePtype;
    /**
     * すべての手を生成する
     */
    template<Player P>
    void AllMoves::
    generateOnBoard(const EffectState& state, Store& action){
      generatePtype<P,PAWN>(state,action);
      generatePtype<P,LANCE>(state,action);
      generatePtype<P,KNIGHT>(state,action);
      generatePtype<P,SILVER>(state,action);
      generatePtype<P,GOLD>(state,action);
      generatePtype<P,BISHOP>(state,action);
      generatePtype<P,ROOK>(state,action);
      PieceOnBoard<>::generatePtype<P,KING>(state,state.kingPiece<P>(),action);
    }
    /**
     * すべての手を生成する
     */
    template<Player P>
    void AllMoves::
    generate(const EffectState& state, Store& action){
      generateOnBoard<P>(state,action);
      Drop::generate<P>(state,action);
    }


  } // namespace move_generator
} // namespace osl

namespace osl
{
  namespace move_generator
  {
    template void AllMoves::generate<BLACK>(EffectState const&,MoveStore&);
    template void AllMoves::generate<WHITE>(EffectState const&,MoveStore&);
    // template void AllMoves::generate(Player,EffectState const&,MoveStore&);
  }
} // namespace osl



