#ifndef CHECKMATE_H
#define CHECKMATE_H
#include "state.h"
#include "more.h"
// immediateCheckmate.h

namespace osl
{
  namespace checkmate
  {
    class ImmediateCheckmate
    {
    private:
      template<Player P>
      static bool hasCheckmateDrop(EffectState const& state,Square target,
				   King8Info mask,Move& win_move);

    public:
      template<Player P>
      static bool slowHasCheckmateMoveDirPiece(EffectState const& state,Square target,
					       King8Info mask,Direction d,Square pos,Piece p,Ptype ptype,Move& win_move);

      template<Player P>
      static bool hasCheckmateMoveDirPiece(EffectState const& state,Square target,
					   King8Info mask,Direction d,Square pos,Piece p,Move& win_move);

      template<Player P>
      static bool hasCheckmateMoveDir(EffectState const& state,Square target,
				      King8Info mask,Direction d,Move& win_move);

      template<Player P>
      static bool hasCheckmateMove(EffectState const& state,Square target,
				   King8Info mask,Move& win_move);

      /**
       * 一手詰めがある局面かどうか判定(move).
       * 手番の側に王手がかかっている場合は除く
       * 長い利きによる王手は生成しない．
       * pinされている駒の利きがないために詰みになる例も扱わない．
       * @param P(template) - 攻撃側(手番側)のプレイヤー
       * @param state - 局面
       * @param best_move - ある場合に詰めの手を返す
       */
      template<Player P>
      static bool hasCheckmateMove(EffectState const& state,Move &win_move);
      template<Player P>
      static bool hasCheckmateMove(EffectState const& state, 
				   King8Info canMoveMask,
				   Square king, Move& win_move);
      /**
       *
       */
      static bool hasCheckmateMove(Player pl,EffectState const& state);
      static bool hasCheckmateMove(Player pl,EffectState const& state,Move& win_move);

    };
  } // namespace checkmate
  using checkmate::ImmediateCheckmate;
} // namespace osl
namespace osl
{
  namespace checkmate
  {
    class ImmediateCheckmateTable
    {
    private:
      CArray<unsigned char,0x10000u> dropPtypeMasks;
      CArray2d<unsigned char,0x100u,Ptype_SIZE> ptypeDropMasks;
      CArray2d<unsigned char,Ptype_SIZE,8> blockingMasks;
      CArray2d<unsigned short,Ptype_SIZE,8> noEffectMasks;
    public:
      ImmediateCheckmateTable();
      unsigned char dropPtypeMaskOf(unsigned int liberty_drop_mask) const
      {
	return dropPtypeMasks[liberty_drop_mask];
      }
      unsigned char dropPtypeMask(King8Info canMoveMask) const
      {
	return dropPtypeMaskOf(libertyDropMask(canMoveMask));
      }
      unsigned int ptypeDropMask(Ptype ptype,King8Info canMoveMask) const
      {
	return ptypeDropMasks[liberty(canMoveMask)][idx(ptype)];
      }
      unsigned int blockingMask(Ptype ptype,Direction dir) const
      {
	assert(static_cast<int>(dir)<8);
	return blockingMasks[idx(ptype)][dir];
      }
      unsigned int noEffectMask(Ptype ptype,Direction dir) const
      {
	assert(static_cast<int>(dir)<8);
	return noEffectMasks[idx(ptype)][dir];
      }
    };
    extern const ImmediateCheckmateTable Immediate_Checkmate_Table;
  }
}

// additionalOrShadow
namespace osl
{
  namespace effect_util
  {
    struct AdditionalOrShadow
    {
      template <int count_max>
      static int count(const PieceVector& direct_pieces, 
		       const EffectState& state,
		       Square target, Player attack)
      {
	int result=0;
	for (Piece p: direct_pieces)
	{
	  const Square from = p.square();
	  int num = p.id();
	  const Direction long_d=to_long_direction<BLACK>(to_offset32(target,from));
	  if(!is_long(long_d)) continue; // unpromoted Knightを除いておくのとどちらが得か?
	  Direction d=long_to_base8(long_d);
	  for(;;){
	    num=state.ppLongState()[num][d];
	    if(Piece::isEmptyNum(num) || state.pieceOf(num).owner()!=attack)
	      break;
	    if (++result >= count_max)
	      return result;
	  }
	}
	return result;
      }

    };
  }
} // namespace osl

namespace osl
{
  namespace move_generator
  {
    /**
     * 利きをつける手を生成 利きを持つstateでしか使えない.
     * アルゴリズム:
     * \li 利きをつけたいマスから8近傍方向(長い利きも)，桂馬近傍の自分の利きをチェック
     * \li 自分の利きがあった時に，そこに移動したら問題のマスに利きをつけられる駒の種類かをチェ
ックする
     * 特徴:
     * \li 相手玉の自由度が小さく，近傍に自分の利きがない時は高速に判定
     * isAttackToKing == true の時
     *  既に王手がかかっている状態は扱わない
     *  自殺手は生成しない?
     * isAttackToKing == false の時
     *  Additional Effect(利きが付いている方向の後ろに長い利きを足す)はいくつでも扱う．
     *  Shadow Effect(相手の利きが付いている方向の後ろに味方の長い利きを足す)は相手が1つの時だけ足す．
     *  自殺手は生成しない．
     */
    class AddEffect
    {
      typedef Store Action;
    public:
      template<Player P>
      static void generate(const EffectState& state,Square target,Action& action,bool& hasPawnCheckmate);
      template<Player P>
      static void generate(const EffectState& state,Square target,Action& action){
        bool dummy;
        generate<P>(state,target,action,dummy);
      }
    };
  }
}

namespace osl
{
  bool win_if_declare(const EffectState& state);
}

#endif
/* CHECKMATE_H */
