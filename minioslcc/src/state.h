#ifndef MINIOSL_STATE_H
#define MINIOSL_STATE_H

#include "basic-type.h"
#include <type_traits>
#include <vector>


namespace osl
{
  enum Handicap{
    HIRATE,
    //    KYOUOCHI,
    //    KAKUOCHI,
  };
  class BaseState;
  std::ostream& operator<<(std::ostream& os,const BaseState& state);
  /**
   * equality allow permutation in piece ids
   */
  bool operator==(const BaseState& st1,const BaseState& st2);

  /** state without piece covers */
  class alignas(32) BaseState
  {
  private:
    friend std::ostream& operator<<(std::ostream& os,const BaseState& state);
    friend bool operator==(const BaseState& st1,const BaseState& st2);
    typedef BaseState state_t;
  protected:
    CArray<Piece,Square::SIZE> board;
    /**
     * 全てのpieceが登録されている
     */
    CArray<Piece,Piece::SIZE> pieces;
    CArray<PieceMask,2> stand_mask;
    CArray<BitXmask,2> pawnMask;
    CArray<CArray<char,basic_idx(Ptype(Ptype_SIZE))>,2> stand_count;

    /** 手番 */
    Player player_to_move;
    PieceMask used_mask;
  public:
    explicit BaseState();
    explicit BaseState(Handicap h);
    // public継承させるので，virtual destructorを定義する．
    virtual ~BaseState();

    const Piece pieceOf(int num) const { return pieces[num]; }
    inline auto all_pieces() const {
      return std::views::iota(0, Piece::SIZE)
        | std::views::transform([this](int n) { return this->pieceOf(n); });
    }
    inline auto long_pieces() const {
      return std::views::iota(ptype_piece_id[idx(LANCE)].first, Piece::SIZE)
        | std::views::transform([this](int n) { return this->pieceOf(n); });
    }
    /**
     * @param sq は isOnboardを満たす Square の12近傍(8近傍+桂馬の利き)
     * ! isOnBoard(sq) の場合は Piece_EDGE を返す
     */
    Piece pieceAt(Square sq) const { return board[sq]; }
    Piece operator[](Square sq) const { return pieceAt(sq); }
    Piece pieceOnBoard(Square sq) const {
      assert(sq.isOnBoard());
      return pieceAt(sq);
    }
    bool isOnBoard(int id) const { return pieceOf(id).isOnBoard(); }

    template<Player P>
    const Piece kingPiece() const { return pieceOf(king_piece_id(P)); }
    const Piece kingPiece(Player P) const { return pieceOf(king_piece_id(P)); }
    template<Player P>
    Square kingSquare() const { return kingPiece<P>().square(); }
    Square kingSquare(Player P) const { return kingPiece(P).square(); }

    const PieceMask& standMask(Player p) const { return stand_mask[p]; }
    const PieceMask& usedMask() const {return used_mask;}

    bool pawnInFile(Player player, int x) const { return bittest(pawnMask[player], x); }
    template<Player P>
    bool pawnInFile(int x) const { return pawnInFile(P,x); }

    Player turn() const { return player_to_move; }
    /**
     * 手番を変更する
     */
    void changeTurn() { player_to_move = alt(player_to_move); }
    void setTurn(Player player) { player_to_move=player; }

    /** (internal) */
    const Piece* getPiecePtr(Square sq) const { return &board[sq]; }
    /** lightweight validation of ordinary moves, primary intended for game record parsing.
     * @see EffectState::isLegalLight for piece reachability. */
    bool move_is_consistent(Move move) const;
    /**
     * 持駒の枚数を数える
     */
    int countPiecesOnStand(Player pl,Ptype ptype) const {
      assert(is_basic(ptype));
      return stand_count[pl][basic_idx(ptype)];
    }
    bool hasPieceOnStand(Player player,Ptype ptype) const { return countPiecesOnStand(player, ptype); }
    template<Ptype T>
    bool hasPieceOnStand(Player P) const { return countPiecesOnStand(P, T); }

    /**
     * @param from - マスの位置
     * @param to - マスの位置
     * @param offset - fromからtoへのshort offset
     * fromとtoがクイーンで利きがある位置関係にあるという前提
     * で，間が全部空白かをチェック
     * @param pieceExistsAtTo - toに必ず駒がある (toが空白でも動く)
     */
    bool isEmptyBetween(Square from, Square to,Offset offset,bool pieceExistsAtTo=false) const {
      assert(from.isOnBoard());
      assert(offset != Offset_ZERO);
      assert(offset==basic_step(to_offset32(to,from)));
      Square sq=from+offset;
      for (; pieceAt(sq).isEmpty(); sq+=offset) {
        if (!pieceExistsAtTo && sq==to) 
          return true;
      }
      return sq==to;
      
    }
    // edit board
    /** set predefined initial state */
    void init(Handicap h);
    /** make empty board ready for manual initialization via setPiece() */
    void initEmpty();
    void initFinalize();

    void setPiece(Player player,Square sq,Ptype ptype);
    void setPieceAll(Player player);

    BaseState rotate180() const;

    bool check_internal_consistency() const;
  protected:
    void setBoard(Square sq,Piece piece) { board[sq]=piece; }
    void clearPawn(Player pl,Square sq) { clear_x(pawnMask[pl], sq); }
    void setPawn(Player pl,Square sq) { set_x(pawnMask[pl], sq); }
  private:
    int countPiecesOnStandBit(Player pl,Ptype ptype) const {
      return (standMask(pl) & PieceMask(piece_id_set(ptype))).countBit();
    }
  };  

} // namespace osl

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

// numEffectState.h
namespace osl
{
  namespace checkmate
  {
    enum King8Info : uint64_t;
  }
  using checkmate::King8Info;
  
  // need to replace when we enable game-tree search again.
  typedef std::vector<Move> MoveVector;
  std::ostream& operator<<(std::ostream&, const MoveVector&);
  typedef std::vector<Piece> PieceVector;
  class EffectState;
  /**
   * equality independent of piece ids
   */
  bool operator==(const EffectState& st1, const EffectState& st2);

  /**
   * 利きを持つ局面
   * - effects (EffectSummary) 利き
   * - pieces_onboard (PieceMask) 盤上にある駒
   */
  class EffectState : public BaseState
  {
    EffectSummary effects;
    CArray<PieceMask,2> pieces_onboard;
    /** 成駒一覧 */
    PieceMask promoted;
    CArray<PieceMask,2> pin_or_open;
    CArray<KingVisibility,2> king_visibility;
    CArray<King8Info,2> king8infos;

    friend bool operator==(const EffectState& st1,const EffectState& st2);
    typedef EffectState state_t;
  public:
    // ----------------------------------------------------------------------
    // 0. 将棋以外の操作
    // ----------------------------------------------------------------------
    explicit EffectState(const BaseState& st=BaseState(HIRATE));
    ~EffectState();
    /** 主要部分を高速にコピーする. 盤の外はコピーされない*/
    void copyFrom(const EffectState& src);
    bool check_internal_consistency() const;

    // ----------------------------------------------------------------------
    // 1. 盤面全体の情報
    // ----------------------------------------------------------------------
    PieceMask piecesOnBoard(Player p) const { return pieces_onboard[p]; }
    PieceMask promotedPieces() const { return promoted; }
    PieceMask pin(Player king) const {
      return pin_or_open[king]&piecesOnBoard(king);
    }
    /** attack の駒で動くと開き王手になる可能性がある集合 */
    PieceMask checkShadow(Player attack) const {
      return pin_or_open[alt(attack)] & piecesOnBoard(attack);
    }
    PieceMask pinOrOpen(Player king) const { return pin_or_open[king]; }
    King8Info king8Info(Player king) const { return king8infos[king]; }
    /** Pの玉が王手状態 */
    bool inCheck(Player P) const {
      const Square king = kingSquare(P);
      if (king.isPieceStand())
        return false;
      return hasEffectAt(alt(P), king);
    }
    /** 手番の玉が王手状態 */
    bool inCheck() const { return inCheck(turn()); }
    /** 手番の玉が詰み (負け) */
    bool inCheckmate() const;
    /**
     * target の王に合駒可能でない王手がかかっているかどうか.
     * - 両王手 => 真
     * - unblockable な利きだけ => 真
     * - blockable な利きだけ => 偽
     * - 王手でない => 偽
     * 2014/03
     */
    bool inUnblockableCheck(Player target) const {
      const Square king_position = kingSquare(target);
      Piece attacker_piece;
      if (hasEffectAt(alt(target), king_position, attacker_piece)) {
        if (attacker_piece == Piece::EMPTY())
          return true;	// multiple pieces
        // sigle check
        Square from = attacker_piece.square();
        EffectDirection effect = ptype_effect(attacker_piece.ptypeO(), from, king_position);
        return is_definite(effect);
      }
      return false; // no check
    }
    const PPLongState& ppLongState() const { return effects.pp_long_state; }
    /** pl からの利きが(1つ以上)ある駒一覧 */
    PieceMask effectedPieces(Player pl) const { return effects.e_pieces[pl]; }
    /** 前の指手でeffectedPieces(pl)が変化したか.
     * 取られた駒は現在の実装ではリストされないようだ.
     */
    PieceMask effectedChanged(Player pl) const { return effects.e_pieces_modified[pl]; }
    bool hasChangedEffects() const { return effects.hasChangedEffects(); }
    BoardMask changedEffects(Player pl) const {
      assert(hasChangedEffects());
      return effects.board_modified[pl];
    }
    BoardMask changedEffects() const { return changedEffects(BLACK) | changedEffects(WHITE); }
    EffectPieceMask changedSource() const { return effects.source_pieces_modified; }
    template <Ptype PTYPE> bool longEffectChanged() const {
      return changedSource().hasLong<PTYPE>();
    }
    template <Ptype PTYPE> bool anyEffectChanged() const {
      return changedSource().hasAny<PTYPE>();
    }
    /** 取られそうなPの駒で価値が最大のもの */
    const Piece findThreatenedPiece(Player P) const;

    template<Player P>
    BoardMask kingArea3x3() { return BoardMaskTable3x3[kingSquare<P>()]; };
    // ----------------------------------------------------------------------
    // 2. 駒に関する情報
    // ----------------------------------------------------------------------
    Square pieceReach(Direction d, int num) const {
      return effects.long_piece_reach.get(d,num);
    }
    Square pieceReach(Direction d, Piece p) const  {
      return pieceReach(d, p.id());
    }
    Square kingVisibilityBlackView(Player p, Direction d) const {
      return Square::makeDirect(king_visibility[p][d]);
    }
    /** 
     * 玉がd方向にどこまで動けるかを返す
     * @param p 注目する玉のプレイヤ
     * @param d piece からみた向き
     */
    Square kingVisibilityOfPlayer(Player p, Direction d) const {
      if (p == BLACK)
        d = inverse(d);
      return kingVisibilityBlackView(p, d);
    }
    /**
     * pinされた駒がPのKingから見てどの方向か?
     * Pから見たdirectionを返す
     */
    template<Player P>
    Direction pinnedDir(Piece p) const {
      assert(p.owner() == P);
      assert(pinOrOpen(P).test(p.id()));
      return base8_dir<P>(p.square(), kingSquare<P>());
    }
    Direction pinnedDir(Piece p) const {
      if (p.owner() == BLACK)
        return pinnedDir<BLACK>(p);
      else
        return pinnedDir<WHITE>(p);
    }
    /**
     * pinされた駒pがtoに動けるか?
     * pinに関係がなければtoへ動けるという前提
     */
    template<Player P>
    bool pinnedCanMoveTo(Piece p,Square to) const {
      assert(p.owner() == P);
      Direction d=pinnedDir<P>(p);
      Square from=p.square();
      return primary(d)==primary(base8_dir_unsafe<P>(from,to));
    }
    bool pinnedCanMoveTo(Piece p,Square to) const {
      if (p.owner() == BLACK)
        return pinnedCanMoveTo<BLACK>(p, to);
      else
        return pinnedCanMoveTo<WHITE>(p, to);
    }
    // ----------------------------------------------------------------------
    // 3. あるSquareへの利き
    // ----------------------------------------------------------------------
    EffectPieceMask effectAt(Square sq) const { return effects.effectAt(sq); }
    PieceMask effectAt(Player P, Square sq) const { return effectAt(sq) & piecesOnBoard(P); }
    /**
     * 利きの数を数える. 
     * targetが盤をはみ出してはいけない
     */
    int countEffect(Player player,Square target) const {
      assert(target.isOnBoard());
      return effectAt(target).countEffect(player);
    }
    /**
     * 利きの数を数える. 
     * targetが盤をはみ出してはいけない
     * @param pins この駒の利きは数えない
     */
    int countEffect(Player player,Square target, PieceMask pins) const {
      assert(target.isOnBoard());
      const EffectPieceMask effect = effectAt(target);
      const int all = effect.countEffect(player);
      pins &= effect;
      return all - pins.countBit();
    }

    // ----------------------------------------------------------------------
    // 3.1 集合を返す
    // ----------------------------------------------------------------------
    template <Ptype PTYPE>
    mask_t ptypeEffectAt(Player P, Square target) const {
      return effectAt(target).selectBit<PTYPE>() & piecesOnBoard(P).to_ullong();
    }
    template <Ptype PTYPE> const mask_t longEffectAt(Square target) const {
      return effectAt(target).selectLong<PTYPE>() >> 8;
    }
    template <Ptype PTYPE> const mask_t longEffectAt(Square target, Player owner) const {
      return longEffectAt<PTYPE>(target) & piecesOnBoard(owner).to_ullong();
    }
    mask_t longEffectAt(Square target) const { return effectAt(target).selectLong() >> 8; }
    mask_t longEffectAt(Square target, Player owner) const  {
      return longEffectAt(target) & piecesOnBoard(owner).to_ullong();
    }

    // ----------------------------------------------------------------------
    // 3.2 bool を返す
    // ----------------------------------------------------------------------
    /** 
     * 対象とするマスにあるプレイヤーの利きがあるかどうか.
     * @param player 攻撃側
     * @param target 対象のマス
     */
    template<Player P>
    bool hasEffectAt(Square target) const {
      assert(target.isOnBoard());
      return effectAt(target).hasEffect<P>();
    }
    /** 
     * 対象とするマスにあるプレイヤーの利きがあるかどうか.
     * @param player 攻撃側
     * @param target 対象のマス
     */
    bool hasEffectAt(Player player,Square target) const {
      assert(target.isOnBoard());
      return effectAt(target).hasEffect(player);
    }
    
    /** 
     * 駒attack が target に利きを持つか
     * @param target 対象のマス
     */
    bool hasEffectByPiece(Piece attack, Square target) const {
      assert(attack.isPiece());
      assert(target.isOnBoard());
      return effectAt(target).test(attack.id());
    }

    /**
     * あるマスにPTYPEの長い利きがあるかどうか.
     */
    template <Ptype PTYPE>
    bool hasLongEffectAt(Player P, Square to) const {
      static_assert((PTYPE == LANCE || PTYPE == BISHOP || PTYPE == ROOK), "ptype");
      return longEffectAt<PTYPE>(to, P);
    }

    /** 
     * 対象とするマスにあるプレイヤーの(ただしある駒以外)利きがあるかどうか.
     * @param player 攻撃側
     * @param piece 攻撃側の駒
     * @param target 対象のマス
     */
    bool hasEffectNotBy(Player player,Piece piece,Square target) const {
      assert(piece.owner()==player);
      PieceMask pieces_onboard=piecesOnBoard(player);
      pieces_onboard.reset(piece.id());
      return (pieces_onboard&effectAt(target)).any();
    }
    /**
     * pinされている駒以外からの利きがある.
     */
    bool hasEffectByNotPinned(Player pl,Square target) const {
      assert(target.isOnBoard());
      PieceMask m=piecesOnBoard(pl)& ~pinOrOpen(pl) & effectAt(target);
      return m.any();
    }

    /**
     * attackerにptypeoの駒がいると仮定した場合にtargetに利きがあるかどうか
     * を stateをupdateしないで確かめる.
     * targetSquareは空白でも良い
     * 盤上の駒を動かす事を検討しているときはに，
     * 自分自身の陰に入って利かないと見なされることがある
     */
    bool hasEffectIf(PtypeO ptypeo,Square attacker, Square target) const {
      Offset32 offset32=to_offset32(target,attacker);
      EffectDirection effect=ptype_effect(ptypeo,offset32);
      if (! any(effect)) 
        return false;
      if (is_definite(effect))
        return true;
      assert(basic_step(offset32) == to_offset(effect));
      return this->isEmptyBetween(attacker,target,to_offset(effect));
    }
    /**
     * 
     */
    template<Player P>
    bool hasEffectByWithRemove(Square target,Square removed) const;
    bool hasEffectByWithRemove(Player player, Square target,Square removed) const {
      if (player==BLACK)
        return hasEffectByWithRemove<BLACK>(target,removed);
      else
        return hasEffectByWithRemove<WHITE>(target,removed);
    }

    // ----------------------------------------------------------------------
    // 3.3 pieceを探す
    // ----------------------------------------------------------------------
    /**
     * 王手駒を探す
     * @return 王手かどうか
     * @param attack_piece
     * 一つの駒による王手の場合はattck_pieceにその駒を入れる
     * 複数の駒による王手の場合はPiece::EMPTY()を入れる
     * @param P(template) 玉
     */
    template<Player P>
    bool findCheckPiece(Piece& attack_piece) const {
      return hasEffectAt<alt(P)>(kingSquare(P),attack_piece);
    }
    bool hasEffectAt(Player P, Square target,Piece& attackerPiece) const {
      if (P == BLACK)
        return hasEffectAt<BLACK>(target, attackerPiece);
      else
        return hasEffectAt<WHITE>(target, attackerPiece);
    }
    /**
     * @param P(template) - 利きをつけている側のプレイヤ
     * @param target - 利きをつけられた場所
     * @param attackerPiece - multiple attackの場合はPiece::EMPTY()
     *        そうでないなら利きをつけている駒を返す
     */
    template<Player P>
    bool hasEffectAt(Square target,Piece& attackerPiece) const {
      attackerPiece=Piece::EMPTY();
      const PieceMask& pieceMask=piecesOnBoard(P)&effectAt(target);
      mask_t mask=pieceMask.to_ullong();
      if (mask == 0) return false;
      /**
       * mask|=0x8000000000000000ll;
       * if((mask&(mask-1))!=0x8000000000000000ll) なら1つのif文で済む
       */
      if (PieceMask(mask).hasMultipleBit())
        return true;
      int num=std::countr_zero(mask);
      attackerPiece=pieceOf(num);
      return true;
    }
    /**
     * pieceのd方向から長い利きがある場合にその駒を返す。
     * @param d piece からみた向き
     */
    Piece findLongAttackAt(Player owner, int piece, Direction d) const {
      assert(pieceOf(piece).isOnBoardByOwner(owner));
      d = change_view(owner, d);
      int num = effects.pp_long_state[piece][d];
      return (num == Piece_ID_EMPTY) ? Piece::EMPTY() : pieceOf(num);
    }
    Piece findLongAttackAt(Player owner, Piece p, Direction d) const {
      return findLongAttackAt(owner, p.id(), d);
    }
    /**
     * 利きの中から安そうな駒を選ぶ
     */
    Piece selectCheapPiece(PieceMask effect) const;
    /**
     * @param P - 利きをつけている側のプレイヤ
     * @param square - 調査する場所
     * @return 利きを付けている中で安そうな駒 (複数の場合でもEMPTYにはしない)
     */
    Piece findCheapAttack(Player P, Square square) const {
      return selectCheapPiece(piecesOnBoard(P) & effectAt(square));
    }
    /**
     * @param P - 利きをつけている側のプレイヤ
     * @param square - 調査する場所
     * @return 利きを付けている中で安そうな駒 (複数の場合でもEMPTYにはしない)
     */
    Piece findCheapAttackNotBy(Player P, Square square, const PieceMask& ignore) const {
      PieceMask pieces = piecesOnBoard(P);
      pieces &= ~ignore;
      return selectCheapPiece(pieces & effectAt(square));
    }
    Piece findAttackNotBy(Player P, Square square, const PieceMask& ignore) const  {
      PieceMask pieces = piecesOnBoard(P);
      pieces &= ~ignore;
      pieces &= effectAt(square);
      if (pieces.none())
        return Piece::EMPTY();
      return pieceOf(pieces.takeOneBit());
    }

    // ----------------------------------------------------------------------
    // 4. 指手の検査・生成・適用
    // ----------------------------------------------------------------------
    /**
     * legal move
     * - valide win declaration, or
     * - move.is_ordinary_valid + isLegalLight + isSafe + !isPawnDropCheckmate
     * todo
     * - evasion in check
     * - repetition of states
     */
    bool isLegal(Move move) const;
    bool isSafeMove(Move move) const;
    bool isCheck(Move move) const;
    bool isPawnDropCheckmate(Move move) const;
    bool isDirectCheck(Move move) const;
    bool isOpenCheck(Move move) const;

    /**
     * moves accepted by makeMove(), a bit different from isLegal
     * - allowed: pass, ordinary moves consistent with state (regardless of king safety)
     * - not allowed: win declaration, resign, 
     */
    bool isAcceptable(Move move) const;

    /**
     * ほぼ全ての合法手を生成する. 玉の素抜きや打歩詰の確認をする．
     * ただし, 打歩詰め絡み以外では有利にはならない手
     * （Move::ignoredUnpromote）は生成しない.
     */
    void generateLegal(MoveVector&) const;
    /**
     * 打歩詰め絡み以外では有利にはならない手も含め, 全ての合法手を生成す 
     * る（Move::ignoredUnpromoteも生成する）. 玉の素抜きや打歩詰の確認
     * をする．
     */
    void generateWithFullUnpromotions(MoveVector&) const;
    /** 王手生成 */
    void generateCheck(MoveVector& moves) const;
    /** 1手詰めの手を見つけられれば生成 */
    Move tryCheckmate1ply() const;

    void makeMove(Move move);
    void makeMovePass() {
      changeTurn();
    }

    // ----------------------------------------------------------------------
    // 5. forEachXXX
    // ----------------------------------------------------------------------
  private:
    template<class Function>
    void forEachEffect(const PieceMask& pieces, Square sq, Function & f) const {
      for (auto num: pieces.toRange()) {
        f(pieceOf(num),sq);
      }
    }      
  public:
    /** 
     * sq への利きを持つ各駒に関して処理を行う.
     */
    template<Player P,class Action>
    void forEachEffect(Square sq,Action & action) const {
      const PieceMask pieceMask=piecesOnBoard(P)&effectAt(sq);
      forEachEffect<Action>(pieceMask, sq, action);
    }
    /** 
     * sq にある駒を取る move を生成して action の member を呼び出す.
     * @param pin 無視する駒
     */
    template<Player P,class Action>
    void forEachEffect(Square sq,Action & action,const PieceMask& pin) const {
      PieceMask pieceMask=piecesOnBoard(P)&effectAt(sq);
      pieceMask &= ~pin;
      forEachEffect<Action>(pieceMask, sq, action);
    }

    /** 
     * sq に移動する move を生成して action の member を呼び出す
     * @param action たとえば AlwayMoveAction
     * @param piece  これ以外の駒を使う
     */
    template<Player P,class Action>
    void forEachEffectNotBy(Square sq,Piece piece,Action & action) const {
      PieceMask pieces=piecesOnBoard(P)&effectAt(sq);
      pieces.reset(piece.id());
      forEachEffect<Action>(pieces, sq, action);
    }

  public:
    /**
     * 玉の素抜きなしに合法手でtargetに移動可能かを判定
     * @param king 玉 (玉で取る手は考えない)
     * @return 移動可能な駒があれば，安全な駒を一つ．なければ Piece::EMPTY()
     * @see osl::move_classifier::PawnDropCheckmate
     */
    template <Player P>
    Piece safeCaptureNotByKing(Square target, Piece king) const;
    Piece safeCaptureNotByKing(Player P, Square target) const {
      const Piece king = kingPiece(P);
      if (P == BLACK)
        return this->safeCaptureNotByKing<BLACK>(target, king);
      else
        return this->safeCaptureNotByKing<WHITE>(target, king);
    }
    /**
     * forEachEffect の Player のtemplate 引数を通常の引数にしたバージョン
     * @param P 探す対象の駒の所有者
     * @param pos に利きのある駒を探す
     */
    template <class Action>
    void forEachEffect(Player P, Square pos, Action& a) const {
      if (P == BLACK)
        this->forEachEffect<BLACK>(pos,a);
      else
        this->forEachEffect<WHITE>(pos,a);
    }
    /**
     * target に利きのあるPieceをoutに格納する
     */
    void findEffect(Player P, Square target, PieceVector& out) const;

  private:
    template <Player P>
    void doSimpleMove(Square from, Square to, int promoteMask, Piece old_piece, Piece new_piece, int num);
    /** @return piece_id used */
    template <Player P>
    int doDropMove(Square to, Ptype ptype);

    template<Player P>
    void doCaptureMove(Square from, Square to, Piece target, int promoteMask, Piece old_piece,
                       Piece new_piece, int num, int target_id);
    //
    template<Direction DIR>
    void makePinOpenDir(Square target, PieceMask& pins, PieceMask attack, KingVisibility& king)
    {
      constexpr Offset offset = black_offset(DIR);
      Square sq=target-offset;
      int num;
      while(Piece::isEmptyNum(num=pieceAt(sq).id()))
        sq-=offset;
      king[DIR] =sq.uintValue(); // narrowing
      if (Piece::isEdgeNum(num)) return;
      int num1=ppLongState()[num][DIR];
      if(Piece::isPieceNum(num1) && attack.test(num1)) 
        pins.set(num);
    }
    void updatePinOpen(Square from, Square to, Player P) {
      Direction lastD=recalcPinOpen(from,P);
      recalcPinOpen(to,P,lastD);
    }
    /**
     * @param lastDir from, to で連続して読んだときの、fromの結果。初回は指定無し=UL
     */
    Direction recalcPinOpen(Square changed, Player defense, Direction lastDir=UL)
    {
      Square target=kingSquare(defense);

      if (target.isPieceStand())
        return lastDir;

      const Direction longD=to_long_direction<BLACK>(changed,target);
      if(!is_long(longD) || (longD==lastDir)) return lastDir;
      lastDir=longD;
      Direction shortD=long_to_base8(longD);
      {
        // reset old pins
        Square oldPos=Square::makeDirect(king_visibility[defense][shortD]);
        int oldNum=pieceAt(oldPos).id();
        if(Piece::isPieceNum(oldNum))
          pin_or_open[defense].reset(oldNum);
      }
      const Offset offset = direction_offsets[idx(longD)];
      Square sq=target-offset;
      int num;
      while(Piece::isEmptyNum(num=pieceAt(sq).id()))
        sq-=offset;
      king_visibility[defense][shortD]=static_cast<unsigned char>(sq.uintValue());
      if(Piece::isEdgeNum(num)) return lastDir;
      int num1=ppLongState()[num][shortD];
      if(Piece::isPieceNum(num1) && piecesOnBoard(alt(defense)).test(num1)) 
        pin_or_open[defense].set(num);
      return lastDir;
    }
    void setPinOpen(Player defense);
    template<Player P>
    void makeKing8Info();
  };

  inline bool operator!=(const EffectState& s1, const EffectState& s2)
  {
    return !(s1==s2);
  }  
} // namespace osl

#endif
