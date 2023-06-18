#ifndef MINIOSL_EFFECT_H
#define MINIOSL_EFFECT_H

#include "base-state.h"
#include <vector>

// contents that depend only on those in base-state, i.e., not on EffectState

// boardMask.h
namespace osl 
{
  class BoardMask;
  bool operator==(const BoardMask&, const BoardMask&);
  std::ostream& operator<<(std::ostream&, const BoardMask&);
  /** 11 x 12 */
  class BoardMask
  {
    /** the third one is only for edge */
    CArray<uint64_t,3> contents;
  public:
    BoardMask() { invalidate(); }
    BoardMask(const BoardMask& src) { 
      contents[0] = src.contents[0]; 
      contents[1] = src.contents[1]; 
    }
    BoardMask& operator=(const BoardMask& src) { 
      if (this != &src) {
        contents[0] = src.contents[0]; 
        contents[1] = src.contents[1]; 
      }
      return *this;
    }
    void clear() { contents[0]=contents[1]=0; }
    void invalidate() { contents[0] = static_cast<uint64_t>(-1); }
    bool isInvalid() const { return contents[0] == static_cast<uint64_t>(-1); }
    static int hi(int n) { assert(0 <= (n>>6) && (n>>6) < 3); return n>>6; }
    static constexpr int lo(int n) { return n&63; }
    void set(unsigned int i) { contents[hi(i)] |= one_hot(lo(i)); }
    void set(Square pos) { set(index(pos)); }
    void reset(unsigned int i) { contents[hi(i)] &= ~one_hot(lo(i)); }
    void reset(Square pos) { reset(index(pos)); }
    bool test(unsigned int i) const { return contents[hi(i)] & one_hot(lo(i)); }
    bool test(Square pos) const { return test(index(pos)); }
    bool anyInRange(const BoardMask& mask) const {
      return (contents[0] & mask.contents[0]) || (contents[1] & mask.contents[1]);
    }
    BoardMask& operator|=(const BoardMask& mask) {
      contents[0] |= mask.contents[0];
      contents[1] |= mask.contents[1];
      return *this;
    }
    bool any() const {
      assert(! isInvalid());
      return contents[0] || contents[1];
    }
    Square takeOneBit() {
      assert(! isInvalid() && any());
      if (contents[0])
        return toSquare(take_one_bit(contents[0]));
      return toSquare(take_one_bit(contents[1])+64);
    }
    static constexpr int index(int x,int y) { return x*12+y+1; }
    static int index(Square pos) {
      int v=pos.index();
      return v-((v>>2)&0x3c);
    }
    template <Direction Dir,Player P>
    static constexpr int index_step() {
      constexpr int dx=black_dx(Dir), dy=black_dy(Dir);
      constexpr int val=dx*12+dy;
      if constexpr (P==BLACK) return val;
      else return -val;
    }
    template <Direction Dir,Player P>
    static void advance(int& idx) { idx += index_step<Dir,P>(); }
    template <Direction Dir,Player P>
    static int neighbor_index(Square sq) { return index(sq)+index_step<Dir,P>(); }
    template <Direction Dir,Player P>
    void setNeighbor(Square sq) { set(neighbor_index<Dir,P>(sq)); }
      
    static Square toSquare(int n) { return Square::makeDirect(n+(((n*21)>>8)<<2)); } 
    friend bool operator==(const BoardMask&, const BoardMask&);
  };
  inline const BoardMask operator|(const BoardMask& l, const BoardMask& r) {
    BoardMask result = l;
    result |= r;
    return result;
  }
  inline bool operator==(const BoardMask& l, const BoardMask& r) {
    return l.contents[0] == r.contents[0] && l.contents[1] == r.contents[1];
  }
  /** Square.index() -> BoardMask 中心3x3 の範囲のbitを立てたもの, centeringなし */
  extern const CArray<BoardMask, Square::SIZE> BoardMaskTable3x3;
} // namespace osl


// numBitmapEffect.h
namespace osl 
{
  enum EffectOp { EffectAdd, EffectSub, };
  namespace effect
  {
    /**
     * property for each square.  
     * 現在の定義 (2005/3/4以降)
     * - 0-39 : 0-39の利き (=PieceMask)
     * - 40-47 : 32-39の長い利き
     * - 48-53 : 黒の利きの数 (sentinelを合わせて6bit)
     * - 54-59 : 白の利きの数 (sentinelを合わせて6bit)
     */
    class EffectPieceMask : public PieceMask
    {
    public:
      explicit EffectPieceMask(uint64_t value=0uLL) : PieceMask(value) {
      }
      EffectPieceMask(PieceMask value) : PieceMask(value) {
      }
      template<Player P>
      static PieceMask base_value() {
	EffectPieceMask ret;
	if constexpr (P == BLACK) ret.flip(48);
	else ret.flip(54);
	return ret;
      }
      static EffectPieceMask base_value(Player pl) {
	mask_t mask1=one_hot(54);
	mask1-=one_hot(48);
	mask1&=mask_t(Int(pl));
	mask1+=one_hot(48);
	EffectPieceMask ret(mask1);
	assert((pl==BLACK && ret==base_value<BLACK>()) || (pl==WHITE && ret==base_value<WHITE>()));
	return ret;
      }
      template<Player P>
      static constexpr mask_t counter_mask() {
	if constexpr (P == BLACK) {
	  mask_t mask1=one_hot(54);
	  mask1-=one_hot(48);
	  return mask1;
	} else {
	  mask_t mask1=one_hot(60);
	  mask1-=one_hot(54);
	  return mask1;
	}
      }

      static constexpr mask_t counter_mask(Player pl) {
	mask_t mask1=one_hot(60);
	mask1-=one_hot(48);
	mask1&=mask_t(Int(pl));
	// pl==BLACK -> mask1 = 0
	// pl==WHITE -> mask1 = 0x0fff0000(32bit), 0x0fff000000000000(64bit)
	mask_t mask2=one_hot(54);
	mask2-=one_hot(48);
	// mask2 = 0x3f0000(32bit), 0x3f000000000000(64bit)
	mask1^=mask2;
	// pl==BLACK -> mask1 = 0x3f0000(32bit), 0x3f000000000000(64bit)
	// pl==WHITE -> mask2 = 0x0fc00000(32bit), 0x0fc0000000000000(64bit)
	assert((pl==BLACK && mask1==counter_mask<BLACK>()) || (pl==WHITE && mask1==counter_mask<WHITE>()));
	return mask1;
      }
      int countEffect(Player pl) const {
	int shift=48+(6&Int(pl));
	mask_t mask = to_ullong();
	mask >>= shift;
	mask&=mask_t(0x3f);
	return mask;
      }
      bool hasEffect(Player pl) const { return osl::any(to_ullong() & counter_mask(pl)); }
      template <Player P>
      bool hasEffect() const { return osl::any(to_ullong() & counter_mask<P>()); }
      
      template<Player P>
      static EffectPieceMask make(int piece_id) {
        return base_value<P>() | PieceMask(one_hot(piece_id));
      }
      template<EffectOp OP>
      EffectPieceMask& increment(EffectPieceMask const& rhs) {
	if constexpr (OP == EffectAdd)
	  *this+=rhs;
	else 
	  *this-=rhs;
	return *this;
      }

      static constexpr mask_t long_mask() { return mask_t(0xff0000000000uLL); }
      static constexpr int longToNumOffset=-8;
      static constexpr mask_t long_bits(int num) { return mask_t(0x101) << num; }
      template<Player P>
      static EffectPieceMask makeLong(int piece_id) {
	assert(is_long_piece_id(piece_id));
	return base_value<P>() | PieceMask(long_bits(piece_id));
      }
      static EffectPieceMask makeLong(Player pl, int piece_id) {
	assert(is_long_piece_id(piece_id));
	return base_value(pl) | PieceMask(long_bits(piece_id));
      }

      // utility methods
      mask_t selectLong() const { return to_ullong() & long_mask(); }
      bool hasLong() const { return selectLong(); }
      template <Ptype PTYPE> mask_t selectLong() const {
	return selectLong() & (piece_id_set(PTYPE) << 8);
      }
      template <Ptype PTYPE> bool hasLong() const { return selectLong<PTYPE>().any(); }
      template <Ptype PTYPE> bool hasAny() const { return to_ullong() & piece_id_set(PTYPE); }
    };
  } // namespace effect
  inline auto long_to_piece_id_range(mask_t m) {
    return BitRange(m) | std::views::transform([](int n)
    { return n+effect::EffectPieceMask::longToNumOffset; });
  }
} // namespace osl

// effectedNumTable.h
namespace osl {
  /**
   * 盤面上の駒が「黒から見た」方向に長い利きをつけられている時に，
   * 利きをつけている駒の番号を得る
   * たとえば，Uの時は下から上方向の長い利きがついているものとする．
   * その方向の利きがない場合は (0は使用するので) Piece_ID_EMPTY (0x80) を入れる．
   */
  struct PPLongState : CArray2d<unsigned char,40,8> {
    inline void clear() { this->fill(Piece_ID_EMPTY); }
    inline void clear(int id) { operator[](id).fill(Piece_ID_EMPTY); }
  };
}

namespace osl
{
  // long piece x base8 -> square (uchar)
    /**
     * 駒毎に指定の方向の利きを持つ最後のSquare.
     * 自分の駒への利きも含む
     * EDGEまでいく
     * 方向は「黒」から見た方向に固定
     * そもそもそちらに利きがない場合やSTANDにある場合は0
     */
  struct LongPieceReach : public CArray2d<uint8_t, 8, 4> {
    LongPieceReach() : CArray2d<uint8_t, 8, 4> {0} {}
    void clear() { fill(0); }
    Square get(Direction d, int piece_id) const {
      assert(is_base8(d) && is_long_piece_id(piece_id));
      // 用途は8方向だけど 縦横と斜めの長い利きはないのでケチった
      return Square::makeDirect(operator[](long_piece_idx(piece_id))[Int(d)/2]);
    }
    void set(Direction d, int piece_id, Square dst) {
      assert(is_base8(d) && is_long_piece_id(piece_id));
      operator[](long_piece_idx(piece_id))[Int(d)/2] = dst.uintValue();
    }
  };
  /** furthest piece_id visible the king for each basic 8 direction (black view) */
  struct KingVisibility : public CArray<uint8_t,8> {
    KingVisibility() : CArray<uint8_t,8>{0} {}
  }; 
}

// numSimpleEffect.h
namespace osl 
{
  namespace effect
  {
    class EffectSummary;
    bool operator==(const EffectSummary&,const EffectSummary&);
    std::ostream& operator<<(std::ostream&, const EffectSummary&);
    /**
     * 局面全体の利きデータ.
     */
    struct alignas(16) EffectSummary
    {
      /** effect to each square on board */
      CArray<EffectPieceMask, Square::SIZE> e_squares;
      CArray<BoardMask,2> board_modified; // each player
      /** set of pieces whose effect changed by previous move */
      EffectPieceMask source_pieces_modified; 
      /** effect to each piece on board */
      CArray<PieceMask,2> e_pieces;
      CArray<PieceMask,2> e_pieces_modified;
      /** mobility */
      LongPieceReach long_piece_reach;
      /** piece-vs-piece effect */
      PPLongState pp_long_state;
      /**
       * ある種類の駒が持つ利きを更新する.
       * @param P(template) - ある位置にある駒の所有者
       * @param OP(template) - 利きを足すか，減らすか
       * @param state - 盤面(動かした後)
       * @param ptypeo - 駒の種類
       * @param pos - 駒の位置
       * @param num - 駒番号
       */
      template<Player P,EffectOp OP>
        void doEffect(const BaseState& state,PtypeO ptypeo,Square pos,int num);

      /**
       * ある駒が持つ利きを更新する.
       * @param OP(template) - 利きを足すか，減らすか
       * @param state - 盤面(動かした後)
       * @param p - 駒
       */
      template<EffectOp OP>
        void doEffect(const BaseState& state,Piece p) {
        if (p.owner() == BLACK)
          doEffect<BLACK,OP>(state,p.ptypeO(),p.square(),p.id());
        else
          doEffect<WHITE,OP>(state,p.ptypeO(),p.square(),p.id());
      }
      /**
       * 盤面のデータを元に初期化する.
       * @param state - 盤面
       */
      void init(const BaseState& state);
      /**
       * コンストラクタ.
       */
      EffectSummary(const BaseState& state) { init(state); }
      /**
       * ある位置の利きデータを取り出す.
       * @param pos - 位置
       */
      EffectPieceMask effectAt(Square pos) const { return e_squares[pos]; }
      /**
       * posに駒を設置/削除して長い利きをブロック/延長する際の利きデータの更新.
       * @param OP(template) - 利きを足すか，減らすか
       * @param state - 局面の状態 posに駒を置く前でも後でもよい
       * @param pos - 変化する位置
       */
      template<EffectOp OP>
        void doBlockAt(const BaseState& state,Square pos,int piece_num);
      friend bool operator==(const EffectSummary& et1,const EffectSummary& et2);
      /*
       *
       */
      const BoardMask changedEffects(Player pl) const { return board_modified[pl]; }
      void setSourceChange(EffectPieceMask const& effect) { source_pieces_modified |= effect; }
      void clearPast() {
	board_modified[0].clear();
	board_modified[1].clear();
	source_pieces_modified.resetAll();
	e_pieces_modified[0].resetAll();
	e_pieces_modified[1].resetAll();
      }
      bool hasChangedEffects() const { return ! board_modified[0].isInvalid(); }
      /** 主要部分を高速にコピーする. 盤の外や直前の利きの変化などの情報はコピーされない*/
      void copyFrom(const EffectSummary& src);
    private:
      /**
       * ある位置からある方向に短い利きがある時に，その方向の利きを更新する.
       * @param P(template) - ある位置にある駒の所有者
       * @param T(template) - ある位置にある駒の種類
       * @param D(template) - 駒の所有者の立場から見た方向
       * @param OP(template) - 利きを足すか，減らすか
       * @param pos - 駒の位置
       * @param num - 駒番号
       */
      template<Player P,Ptype T,Direction Dir,EffectOp OP>
        void doEffectShort(const BaseState& state,Square pos,int num);
      /**
       * ある位置からある方向に長い利きがある時に，その方向の利きを更新する.
       * @param P(template) - ある位置にある駒の所有者
       * @param T(template) - ある位置にある駒の種類
       * @param Dir(template) - 黒の立場から見た方向
       * @param OP(template) - 利きを足すか，減らすか
       * @param state - 盤面(動かした後)
       * @param pos - 駒の位置
       * @param num - 駒番号
       */
      template<Player P,Ptype T,Direction Dir,EffectOp OP>
        void doEffectLong(const BaseState& state,Square pos,int num);
      /**
       * ある種類の駒が持つ利きを更新する.
       * @param P(template) - ある位置にある駒の所有者
       * @param T(template) - ある位置にある駒の種類
       * @param OP(template) - 利きを足すか，減らすか
       * @param state - 盤面(動かした後)
       * @param pos - 駒の位置
       * @param num - 駒番号
       */ 
     template<Player P,Ptype T,EffectOp OP>
        void doEffectBy(const BaseState& state,Square pos,int num);      
    };

    inline bool operator!=(const EffectSummary& et1,const EffectSummary& et2)
    {
      return !(et1==et2);
    }

  } // namespace effect
  using effect::EffectPieceMask;
  using effect::EffectSummary;
} // namespace osl

#endif
// MINIOSL_EFFECT_H
