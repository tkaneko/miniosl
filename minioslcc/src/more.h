#ifndef MINIOSL_MORE_H
#define MINIOSL_MORE_H
#include "state.h"
#include <string>
#include <stdexcept>
// piecestand.h
namespace osl
{
  /**
   * 片方の手番の持駒の枚数を記録するクラス.
   * - 一応 king を持駒にして良いことにしておく
   * レイアウト 長さ:index
   * -  reserved : 1;31
   * -  carry    : 1;
   * -  KING     : 2;28
   * -  carry    : 1;
   * -  GOLD     : 3;24
   * -  carry    : 1;
   * -  PAWN     : 5;18
   * -  carry    : 1;
   * -  LANCE    : 3;14
   * -  carry    : 1;
   * -  KNIGHT   : 3;10
   * -  carry    : 1;
   * -  SILVER   : 3; 6
   * -  carry    : 1;
   * -  BISHOP   : 2; 3
   * -  carry    : 1; 
   * -  ROOK     : 2; 0
   *
   * == を軽くするために carry off の状態を基本とする
   */
  class PieceStand
  {
  public:
    static constexpr uint32_t carryMask = 0x48822224;
  private:
    static const CArray<unsigned char,Ptype_MAX+1> shift;
    static const CArray<unsigned char,Ptype_MAX+1> mask;
    mutable uint32_t flags;
  public:
    explicit PieceStand(unsigned int value=0) : flags(value)
    {
    }
    explicit PieceStand(Player, const BaseState&);
    PieceStand(int pawnCount, int lanceCount, 
	       int knightCount, int silverCount,
	       int goldCount, int bishopCount,
	       int rookCount, int kingCount) 
      : flags(0)
    {
      add(PAWN, pawnCount);
      add(LANCE, lanceCount);
      add(KNIGHT, knightCount);
      add(SILVER, silverCount);
      add(GOLD, goldCount);
      add(BISHOP, bishopCount);
      add(ROOK, rookCount);
      add(KING, kingCount);
    }

    void add(Ptype type, unsigned int num=1)
    {
      assert(is_basic(type));
      assert(num == (num & mask[idx(type)]));
      flags += (num << (shift[idx(type)]));
      assert(testCarries() == 0);	// overflow 検出
    }    
    void sub(Ptype type, unsigned int num=1)
    {
      assert(is_basic(type));
      assert(num == (num & mask[idx(type)]));
      assert(get(type) >= num);
      flags -= (num << (shift[idx(type)]));
    }

    /**
     * 加算可能なら加える.
     * 速度が必要なところでは使ってないので .cc に移動．
     */
    void tryAdd(Ptype type);
    bool canAdd(Ptype type) const;
    /**
     * 1枚以上持っていれば減らす
     */
    void trySub(Ptype type)
    {
      if (get(type))
	sub(type);
    }

    /**
     * 一種類の駒しかない
     */
    bool atMostOneKind() const;

    /**
     * pieceStand同士の加算，減算.
     * 足して良いのは，carry が立っていないpiecestandで
     * かつ，含まれる駒が高々1つ
     */
    void addAtmostOnePiece(PieceStand const& ps){
#ifndef NDEBUG
      const PieceStand copy(*this);
#endif
      assert(! ps.testCarries());
      assert(ps.atMostOneKind());
      flags += ps.getFlags();
      assert(carryUnchangedAfterAdd(copy, ps));
    }

    void subAtmostOnePiece(PieceStand const& ps){
#ifndef NDEBUG
      const PieceStand copy(*this);
#endif
      assert(! ps.testCarries());
      assert(ps.atMostOneKind());
      flags -= ps.getFlags();
      assert(carryUnchangedAfterSub(copy, ps));
    }
  private:
    bool carryUnchangedAfterAdd(const PieceStand& original, const PieceStand& other) const;
    bool carryUnchangedAfterSub(const PieceStand& original, const PieceStand& other) const;
  public:
    unsigned int get(Ptype type) const
    {
      return (flags >> (shift[idx(type)])) & mask[idx(type)];
    }
    void carriesOff() const { flags &= (~carryMask); }
    void carriesOn()  const { flags |= carryMask; }
    unsigned int testCarries() const { return (flags & carryMask); }
    bool isSuperiorOrEqualTo(PieceStand other) const
    {
      carriesOn();
      other.carriesOff();
      const bool result = (((flags - other.flags) & carryMask) == carryMask);
      carriesOff();
      return result;
    }
    /**
     * this と other が BLACK の持駒と考えた時に，
     * this の方が同じか沢山持っていれば真.
     */
    template <Player P>
    bool hasMoreThan(PieceStand other) const
    {
      if (P == BLACK)
	return isSuperiorOrEqualTo(other);
      else
	return other.isSuperiorOrEqualTo(*this);
    }
    bool hasMoreThan(Player P, PieceStand other) const
    {
      if (P == BLACK)
	return hasMoreThan<BLACK>(other);
      else
	return hasMoreThan<WHITE>(other);
    }
    unsigned int getFlags() const { return flags; }
    /** どれかの駒を一枚でも持っている */
    bool any() const { return flags; }
    /**
     * 種類毎に this と other の持駒の多い方を取る
     */
    const PieceStand max(PieceStand other) const
    {
      // other以上の数持っているptypeに対応するcarryが1になる．
      const unsigned int mask0 = ((flags|carryMask)-other.flags) & carryMask;
      // ROOK BISHOP KING用のMASKを作る
      unsigned int my_mask = mask0-((mask0&0x40000024)>>2);
      // GOLD SILVER KNIGHT LANCE用のMASKを作る
      my_mask -= (mask0&0x08022200)>>3;
      // PAWN用のMASKのみ残す
      my_mask -= (mask0&0x00800000)>>5;
      // my_mask が1のptypeの数は自分から，0のptypeはotherのところの値を
      return PieceStand((flags&my_mask)|(other.flags&~my_mask));
    }     
    /**
     * 種類毎に this と other の持駒の多い方を取る (max のalternative)
     */
    const PieceStand max2(PieceStand other) const
    {
      // other以上の数持っているptypeに対応するcarryが1になる．
      const unsigned int diff0=((flags|carryMask)-other.flags);
      const unsigned int mask0=diff0&carryMask;

      // ROOK BISHOP KING GOLD SILVER KNIGHT LANCE用のMASKを作る
      const unsigned int mask02=(mask0&0x40000024u)+(mask0&0x48022224u);
      unsigned int my_mask=mask0-(mask02>>3);

      // PAWN用のMASKのみ残す
      my_mask -= (mask0&0x00800000)>>5;
      // my_mask が1のptypeの数は自分から，0のptypeはotherのところの値を
      return PieceStand((other.flags+(diff0&my_mask))&~carryMask);
    }     

    const PieceStand nextStand(Player pl, Move move) const
    {
      assert(move.isNormal());
      PieceStand result = *this;
      if (move.player() == pl)
      {
	if (auto ptype = move.capturePtype(); ptype!=Ptype_EMPTY)
	{
	  result.add(unpromote(ptype));
	}
	else if (move.isDrop())
	{
	  const Ptype ptype = move.ptype();
	  assert(get(ptype));
	  result.sub(ptype);
	}
      }
      return result;
    }
    const PieceStand nextStand(Move move) const
    {
      return nextStand(move.player(), move);
    }
    const PieceStand previousStand(Player pl, Move move) const
    {
      assert(move.isNormal());
      PieceStand result = *this;
      if (move.player() == pl)
      {
	if (Ptype ptype = move.capturePtype(); ptype!=Ptype_EMPTY)
	{
	  const Ptype before = unpromote(ptype);
	  assert(get(before));
	  result.sub(before);
	}
	else if (move.isDrop())
	{
	  const Ptype ptype = move.ptype();
	  result.add(ptype);
	}
      }
      return result;
    }
    const PieceStand previousStand(Move move) const
    {
      return previousStand(move.player(), move);
    }
  };

  inline bool operator==(PieceStand l, PieceStand r)
  {
    assert(! l.testCarries());
    assert(! r.testCarries());
    return l.getFlags() == r.getFlags();
  }
  inline bool operator!=(PieceStand l, PieceStand r)
  {
    return ! (l == r);
  }
  inline bool operator<(PieceStand l, PieceStand r)
  {
    assert(! l.testCarries());
    assert(! r.testCarries());
    return l.getFlags() < r.getFlags();
  }
  std::ostream& operator<<(std::ostream&, PieceStand l);

  struct PieceStandIO
  {
    /**
     * 持駒の数を空白区切で出力する. 数値処理用途
     */
    static std::ostream& writeNumbers(std::ostream&, const PieceStand& stand);
    static std::istream& readNumbers(std::istream&, PieceStand& stand);
  };
} // namespace osl
// additionalEffect.h

namespace osl
{
  /**
   * 追加利きを求める
   */
  struct AdditionalEffect
  {
    /**
     * target に attack の追加利きが一つでもあるか．
     * 相手の影利きが先にある場合は対象としない．
     */
    static bool hasEffect(const EffectState&, Square target, Player attack);
  };
} // namespace osl

// king8Info.h
namespace osl
{
  namespace checkmate {
  /**
   * 敵玉の8近傍の状態を表す. 王手がかかっている場合も含むことにする．
   * Dirは攻撃側が相手の玉に対してDir方向で王手をかける位置
   * 0-7 : 敵玉以外の利きがなく，自分の利きがある空白
   *       (駒を打つ候補となりうる点)
   * 8-15 : 敵玉がDirに移動可能(王手がかかっている場合は長い利きも延ばす)
   * 16-23 : 空白か味方の駒(利き次第では移動可能になる)
   * 24-31 : 敵玉以外の利きがなく，自分の利きがある空白，敵駒
   * 32-39 : 空白(駒打ち王手の候補)
   * 40-47 : 味方の利き(kingの利きも含んでいる)がある空白，敵駒
   * 48-51 : [数] 敵玉がDirに移動可能(王手がかかっている場合は長い利きも延ばす)な数
   */
    enum King8Info : uint64_t {};
    /** 0-7 bit 目を返す */
    constexpr inline unsigned int dropCandidate(King8Info value) { return (value&0xffull); }
    /** 8-15 bit 目を 0-7bitにshiftして返す */
    constexpr inline unsigned int liberty(King8Info value) { return ((value>>8)&0xffull); }
    /** 0-15bit */
    constexpr inline unsigned int libertyDropMask(King8Info value)  { return (value&0xffffull); }
    /** 16-23 bit 目を 0-7bitにshiftして返す */
    constexpr inline unsigned int libertyCandidate(King8Info value) { return ((value>>16)&0xffull); }
    /** 24-31 bit 目を 0-7bitにshiftして返す */
    constexpr inline unsigned int moveCandidate2(King8Info value) { return ((value>>24)&0xffull); }
    /** libertyの数 */
    constexpr inline unsigned int libertyCount(King8Info value) { return ((value>>48)&0xfull); }
    constexpr inline unsigned int spaces(King8Info value) { return ((value>>32)&0xffull); }
    constexpr inline unsigned int moves(King8Info value) { return ((value>>40)&0xffull); }

    template<Player Attack>
    King8Info to_king8info(EffectState const& state, Square king, PieceMask pinned);
    
    template <Player Attack>
    inline King8Info to_king8info(EffectState const& state, Square king) {
      return to_king8info<Attack>(state,king,state.pin(alt(Attack)));
    }
    inline King8Info to_king8info(Player attack, EffectState const& state) {
      Square king=state.kingSquare(alt(attack));
      return (attack == BLACK) ? to_king8info<BLACK>(state, king) : to_king8info<WHITE>(state, king);
    }
    /**
     * alt(P)の玉にDirの方向で迫るcanMoveMaskを計算する.
     * @param P(template) - 攻撃側のplayer
     * @param Dir(template) - 敵玉に迫る方向(shortの8方向)
     * @param state - 初期状態
     * @param target - alt(P)の玉があるpotision
     */
    template<Player P,Direction Dir>
    uint64_t make_king8info(EffectState const& state,Square target, PieceMask pinned,
                            PieceMask on_board_defense);

    std::ostream& operator<<(std::ostream&, King8Info);
  } // checkmate
  using checkmate::to_king8info;
} // namespace osl

// pieceTable
namespace osl
{
  constexpr std::array<Ptype, 40> piece_id_ptype = {{
      PAWN, PAWN, PAWN, PAWN, PAWN, PAWN, PAWN, PAWN, PAWN,
      PAWN, PAWN, PAWN, PAWN, PAWN, PAWN, PAWN, PAWN, PAWN,
      KNIGHT, KNIGHT, KNIGHT, KNIGHT,
      SILVER, SILVER, SILVER, SILVER,
      GOLD, GOLD, GOLD, GOLD,
      KING, KING,
      LANCE, LANCE, LANCE, LANCE,
      BISHOP, BISHOP,
      ROOK, ROOK,
    }};  
}

// move_classifier
namespace osl
{
  namespace move_classifier {
    /** drop の時は呼べないなどの条件を代わりにテスト */
    template <class Classifier>
    struct MC {
      static bool match(const EffectState& state, Move m)  {
	if constexpr (! Classifier::drop_suitable)
          if (m.isDrop())
            return Classifier::result_if_drop;
	assert(m.player() == state.turn());
	if (state.turn() == BLACK)
	  return Classifier::template isMember<BLACK>(state, m.ptype(), m.from(), m.to());
	else
	  return Classifier::template isMember<WHITE>(state, m.ptype(), m.from(), m.to());
      }
    };

    /**
     * Pの王をopen checkにする手でないことをチェック.
     * - P==move playerの時は自殺手かどうかのチェックに使う.
     *      王が動く場合には呼べない
     * - P!=move playerの時は通常のopen checkかどうかに使う.
     * - DropMoveの時には呼べない
     */
    struct KingOpenMove {
      static constexpr bool drop_suitable = false;
      static constexpr bool result_if_drop = false;
      /**
       * king が59
       * rookが51->61の時，差は
       * OFFSET -8 -> U
       * OFFSET +8 -> D
       * とはなるので，一直線のような気がする．ただし，もそとそも，
       * 59 - 51はpinにはならないし，今は U -> DはopenではないとしているのでOK
       */
      template <Player P>
      static bool isMember(const EffectState& state, 
			   Ptype /*ptype*/,Square from,Square to) {
	int num=state.pieceAt(from).id();
	assert(Piece::isPieceNum(num));
	if(!state.pinOrOpen(P).test(num)) return false;
	// from to kingが一直線に並べば false
	Square king=state.kingSquare<P>();
	return base8_dir_unsafe<P>(king,to) != base8_dir_unsafe<P>(king,from);
      }
      /**
       * @param exceptFor ここからの利きは除外
       */
      template <Player P>
      static bool isMember(const EffectState& state, Ptype ptype,Square from,Square to,
			   Square exceptFor);
    };

    /**
     * 元々，手番の玉に王手がかかっていない状態で自殺手でないことをチェック.
     * DropMoveの時には呼べない
     */
    struct SafeMove
    {
      static constexpr bool drop_suitable = false;
      static constexpr bool result_if_drop = true;
      template <Player P>
      static bool isMember(const EffectState& state, 
			   Ptype ptype,Square from,Square to) {
	assert(! from.isPieceStand());
	assert(state.pieceOnBoard(from).owner() == P);
	/**
	 * 元々王手がかかっていないと仮定しているので，自分を
	 * 取り除いた上でhasEffectByを呼ばなくても良い
	 */
	if (ptype==KING)
	  return ! state.hasEffectAt<alt(P)>(to);
	return ! KingOpenMove::isMember<P>(state,ptype,from,to);
      }
    };

    /** 王手判定 Pは攻撃側 */
    struct OpenCheck {
      static constexpr bool drop_suitable = false;
      static constexpr bool result_if_drop = false;
      template <Player P>
      static bool isMember(const EffectState& state, 
			   Ptype ptype,Square from,Square to) {
	return KingOpenMove::isMember<alt(P)>(state,ptype,from,to);
      }
    };

    struct DirectCheck {
      static constexpr bool drop_suitable = true;
      template <Player P>
      static bool isMember(const EffectState& state, Ptype ptype, Square to) {
	/**
	 * 最初から王手ということはない．
	 */
	assert(!state.hasEffectAt<P>(state.kingSquare<alt(P)>()));
	/**
	 * stateを動かしていないので，fromにある駒がtoからの利きを
	 * blockすることは
	 * あるが，blockされた利きが王手だったとすると，動かす前から王手
	 * だったとして矛盾するのでOK
	 */
	return state.hasEffectIf(newPtypeO(P,ptype),to,state.kingSquare<alt(P)>());
      }
      template <Player P>
      static bool isMember(const EffectState& state, Ptype ptype, Square /*from*/, Square to) {
	return isMember<P>(state, ptype, to);
      }
    };
    /**
     * @param 指す側, alt(P)に王手をかけられるか判定
     */
    struct Check {
      static constexpr bool drop_suitable = true;
      /**
       * promote move の時 ptypeはpromote後のもの
       */
      template <Player P>
      static bool isMember(const EffectState& state, 
			   Ptype ptype,Square from,Square to) {
	if (DirectCheck::isMember<P>(state,ptype,to)) 
	  return true;
	if (from.isPieceStand()) 
	  return false;
	return OpenCheck::isMember<P>(state,ptype,from,to);
      }
    };

    /**
     * 打歩詰の判定.
     * @param P 指手(攻撃)側
     */
    struct PawnDropCheckmate {
      static constexpr bool drop_suitable = true;
      /**
       * kingSquare に居る alt(P)の玉が dir 方向に逃げられるか.
       */
      template <Player P>
      static bool is_safe_direction(const EffectState& state, Square kingSquare, 
                                    Direction dir, Square dropAt);
      /** 王が前以外に移動可能か */
      template <Player P>
      static bool escape7(const EffectState& state, Square kingSquare, Square to);
      template <Player P>
      static bool isMember(const EffectState& state, 
			   Ptype ptype,Square from,Square to) {
	// 打歩
	if (! from.isPieceStand() || ptype != PAWN)
	  return false;
	const Player Opponent = alt(P);
	const Piece king = state.kingPiece<Opponent>();
	const Square king_position = king.square();	
        if (king_position != (to + to_offset(P,U)) // 玉頭
            || ! state.hasEffectAt(P, to) // 玉で取れない
            || liberty(King8Info(state.king8Info(Opponent))) != 0
            || state.safeCaptureNotByKing<Opponent>(to, king) != Piece::EMPTY()) // 玉以外の駒で取れない
	  return false;
	// どこにも逃げられない
	return escape7<P>(state, king_position, to);
      }
    };
    inline bool is_safe(const EffectState& state, Move move) { return MC<SafeMove>::match(state, move); }
    inline bool is_check(const EffectState& state, Move move) { return MC<Check>::match(state, move); }
    inline bool is_pawn_drop_checkmate(const EffectState& state, Move move) {
      return MC<PawnDropCheckmate>::match(state, move);
    }
    inline bool is_direct_check(const EffectState& state, Move move) { return MC<DirectCheck>::match(state, move); }
    inline bool is_open_check(const EffectState& state, Move move) { return MC<OpenCheck>::match(state, move); }
  } // namespace move_classifier
  using move_classifier::is_safe;
  using move_classifier::is_check;
  using move_classifier::is_pawn_drop_checkmate;
  using move_classifier::is_direct_check;
  using move_classifier::is_open_check;
} // namespace osl

template <osl::Player P>
bool osl::move_classifier::PawnDropCheckmate::
is_safe_direction(const EffectState& state, Square kingSquare, 
	  Direction dir, Square dropAt) 
{
  const Player Opponent = alt(P);
  const Square target = kingSquare + to_offset<Opponent>(dir);
  const Piece p = state.pieceAt(target);
  if (p.isOnBoardByOwner<Opponent>())
    return false;		// 自分の駒がいたら移動不能
  if (target.isEdge())
    return false;
  Piece attacker;
  if (! state.hasEffectAt<P>(target, attacker))
    return true;		// 利きがない
  if (attacker == Piece::EMPTY())
    return false;		// 攻撃側に複数の利き
  assert(attacker.owner() == P);
  // drop によりふさがれた利きなら逃げられる
  //    -OU
  // XXX+FU+HI
  // の場合のXXXなど．
  const Offset shortOffset = base8_step(target, dropAt);
  if (shortOffset == Offset_ZERO)
    return false;
  const Square attackFrom = attacker.square();
  return shortOffset == base8_step(dropAt,attackFrom);
}

template <osl::Player P>
bool osl::move_classifier::PawnDropCheckmate::
escape7(const EffectState& state, Square king_position, Square to) {
  // U は歩
  return ! is_safe_direction<P>(state, king_position, UL, to)
    && ! is_safe_direction<P>(state, king_position, UR, to)
    && ! is_safe_direction<P>(state, king_position, L, to)
    && ! is_safe_direction<P>(state, king_position, R, to)
    && ! is_safe_direction<P>(state, king_position, DL, to)
    && ! is_safe_direction<P>(state, king_position, D, to)
    && ! is_safe_direction<P>(state, king_position, DR, to);
}

namespace osl
{
  namespace move_action
  {
    /**
     * 指手を MoveVector に保管
     */
    struct Store
    {
      MoveVector& moves;
      explicit Store(MoveVector& v) : moves(v) {}
      void operator()(Square /*to*/, Move move) {
	moves.push_back(move);
      }
      // old interfaces
      void simpleMove(Square from,Square to,Ptype ptype,
                      bool isPromote,Player p)
      {
        operator()(to, Move(from,to,ptype,Ptype_EMPTY,isPromote,p));
      }
      void unknownMove(Square from,Square to,Piece captured,
                       Ptype ptype,bool isPromote,Player p)
      {
        operator()(to, Move(from,to,ptype,captured.ptype(),isPromote,p));
      }
      void dropMove(Square to,Ptype ptype,Player p)
      {
        operator()(to, Move(to,ptype,p));
      }
    };
  } // namespace move_action
  using MoveStore=move_action::Store;
} // namespace osl

namespace osl
{
  namespace effect_action
  {
    /**
     * PieceVector に格納
     */
    struct StorePiece
    {
      PieceVector *store;
      explicit StorePiece(PieceVector *s) : store(s)
      {
      }
      void operator()(Piece p, Square) {
	store->push_back(p);
      }
    };
  } // namespace effect_action
} // namespace osl
namespace osl
{
  namespace move_generator
  {
    /** base のtoをoffsetだけ変える．元のtoが0以外でも使える */
    inline Move adjust_to(Move base, Offset o) { return Move::makeDirect(base.intValue()+Int(o)); }
    /** つくってあったmoveの雛形のsquareをsetする．skeltonのtoは0 */
    inline Move set_skelton_to(Move skelton, Square to) {
      assert((skelton.intValue()&0xff)==0);
      return Move::makeDirect(skelton.intValue()+to.uintValue());
    }
    inline auto drop_skelton(Ptype ptype, Player P) { return Move(Square::STAND(), ptype, P); }

    using osl::move_action::Store;
    /**
     * 打つ手を生成
     */
    class Drop
    {
    public:
      template<Player P>
      static void generate(const EffectState& state,Store& action);
    };
  } // namespace move_generator
} // namespace osl
namespace osl
{
  namespace move_generator
  {
    /**
     * Move::ignoreUnpromote() でないすべての手を生成
     * @param Action move_action
     */
    class AllMoves
    {
    public:
      /**
       * @param P - 手番のプレイヤ
       * state - 手を生成する局面．王手がかかっていないことを想定
       * action - 手正成用のcallback
       */
      template<Player P>
      static void generateOnBoard(const EffectState& state, Store& action);

      /**
       * @param P - 手番のプレイヤ
       * state - 手を生成する局面．王手がかかっていないことを想定
       * action - 手正成用のcallback
       */
      template<Player P>
      static void generate(const EffectState& state, Store& action);

      static void generate(Player p, const EffectState& state, Store& action)
      {
	if(p==BLACK)
	  generate<BLACK>(state,action);
	else
	  generate<WHITE>(state,action);
      }
    };

  } // namespace move_generator
} // namespace osl

namespace osl
{
  namespace move_generator
  {
    /**
     * 駒を取る手を生成
     */
    class Capture
    {
    public:
      /**
       * @param target 取る駒の位置 (can be empty)
       */
      template<Player P>
      static void generate(const EffectState& state,Square target,
			   Store& action);
      /**
       * @param target 取る駒の位置 (can be empty)
       * @param piece  この駒以外で取る
       * before 2009/12/20 pinを考慮していなかった
       */
      template<Player P>
      static void escapeByCapture(const EffectState& state,Square target,
				  Piece piece,Store& action);
    };    
  } // namespace move_generator
} // namespace osl

namespace osl
{
  namespace move_generator
  {
    enum PromoteType{
      NoPromote=0,
      AssuredPromote=1,
      PromoteOnDirection=2,
      MustPromote=3,
    };
    template<Ptype T,Player P> struct PromoteTest {
      /** posにあるTの駒がpromoteする手しかない */
      static bool must_promote(Square pos) {
        if constexpr (P==BLACK) {
          if constexpr (T==PAWN || T==LANCE) return pos.yEq<2>();
          if constexpr(T==KNIGHT) return pos.yLe<4>();
        }
        else{
          if constexpr (T==PAWN || T==LANCE) return pos.yEq<8>();
          if constexpr(T==KNIGHT) return pos.yGe<6>();
        }
        return false;
      }
      /** posにあるTの駒がどの方向に動いてもpromote可能 */
      static bool promote_guaranteed(Square pos) {
        if constexpr (P==BLACK) {
          if constexpr (T==PAWN || T==LANCE) return pos.yLe<4>();
          if constexpr (T==KNIGHT) return pos.yLe<5>();
          return pos.yLe<3>();
        }
        else{
          if constexpr (T==PAWN || T==LANCE) return pos.yGe<6>();
          if constexpr (T==KNIGHT) return pos.yGe<5>();
          return pos.yGe<7>();
        }
      }
      /**
       * posにあるTの駒がpromote可能なdirectionに動く時だけpromote可能
       * shortの時はその時のみYES
       */
      static bool promote_on_direction(Square pos) {
        if constexpr (P==BLACK) {
          if constexpr (T==SILVER) return pos.yEq<4>();
          if constexpr (T==LANCE || T==ROOK || T==BISHOP) return true;
        }
        else{
          if constexpr (T==SILVER) return pos.yEq<6>();
          if constexpr (T==LANCE || T==ROOK || T==BISHOP) return true;
        }
        return false;
      }
      /**
       * posにあるTの駒は次に絶対にpromoteできない
       */
      static bool promote_unable(Square pos) {
        if constexpr (P==BLACK) {
          if constexpr (T==PAWN || T==SILVER) return pos.yGe<5>();
          if constexpr (T==KNIGHT) return pos.yGe<6>();
          if constexpr (T==LANCE || T==ROOK || T==BISHOP) return false;
        }
        else{
          if constexpr (T==PAWN || T==SILVER) return pos.yLe<5>();
          if constexpr (T==KNIGHT) return pos.yLe<4>();
          if constexpr (T==LANCE || T==ROOK || T==BISHOP) return false;
        }
        return true;
      }
    };  
    template <Ptype T, Player P>
    inline PromoteType promote_type(Square sq) {
      if (PromoteTest<T,P>::must_promote(sq)) return MustPromote;
      if (PromoteTest<T,P>::promote_guaranteed(sq)) return AssuredPromote;
      if (PromoteTest<T,P>::promote_on_direction(sq)) return PromoteOnDirection;
      return NoPromote;
    }
    
    /**
     * 特定のpieceを動かす手を生成
     */
    template <bool PlainOnly=false>
    struct PieceOnBoard
    {
      /**
       * 駒pがマスtargetに利きをもっているとして，手を生成する．
       */
      template<Player P>
      static void generatePieceUnsafe(const EffectState& state,Piece p, Square target, Piece p1,MoveStore& action)
      {
        // p1 == dst
	assert(state.hasEffectByPiece(p, target));
	Ptype ptype=p.ptype();
	Square from=p.square();
	if(can_promote(ptype)){
	  if(target.isPromoteArea(P)){
	    action(target, Move(from,target,promote(ptype),p1.ptype(),true,P));
	    int y=(P==BLACK ? target.y() : 10-target.y());
	    if(! ptype_prefer_promote[idx(ptype)] && 
	       (((ptype==LANCE || ptype==PAWN) ? y==3 : true )) &&
	       legal_drop_at(P,ptype,target))
	      action(target, Move(from,target,ptype,p1.ptype(),false,P));
	  }
	  else if(from.isPromoteArea(P)){
	    action(target, Move(from,target,promote(ptype),p1.ptype(), true,P));
	    if(!ptype_prefer_promote[idx(ptype)])
	      action(target, Move(from,target,ptype,p1.ptype(),false,P));
	  }
	  else
	    action(target, Move(from,target,ptype,p1.ptype(),false,P));
	}
	else{
	  action(target, Move(from,target,ptype,p1.ptype(),false,P));
	}
      }
      template<Player P>
      static void generatePiece(const EffectState& state,Piece p, Square target, Piece p1,MoveStore& action)
      {
	if(p.ptype()==KING){
	  // 王手がかかっているときには自分の影になっている手も生成してしまう
	  const Player altP=alt(P);
//	  assert(!state.hasEffectAt<altP>(p.square()));
	  // 自殺手
	  if(state.hasEffectAt<altP>(target)) return;
	}
	if(state.pinOrOpen(P).test(p.id())){
	  Direction d=state.pinnedDir<P>(p);
	  Direction d1=base8_dir_unsafe<P>(p.square(),target);
	  if(primary(d)!=primary(d1)) return;
	}
	generatePieceUnsafe<P>(state,p,target,p1,action);
      }
      /**
       * PtypeがTの駒pがマスtargetに利きをもっているとして，手を生成する．
       * p1 - targetにある駒
       */
      template<Player P,Ptype T>
      static void generatePiecePtypeUnsafe(const EffectState& state,Piece p, Square target, Piece p1,MoveStore& action)
      {
	assert(state.hasEffectByPiece(p, target));
	assert(p.ptype()==T);
//	Ptype ptype=p.ptype();
	Square from=p.square();
	if(can_promote(T) & (target.isPromoteArea(P) || from.isPromoteArea(P))){
	  action.unknownMove(from,target,p1,promote(T),true,P);
	  if constexpr (T==PAWN || T==LANCE)
            if (P==BLACK ? target.y()==1 : target.y()==9)
              return;
          if constexpr (T==KNIGHT)
            if (P==BLACK ? target.y()<=2 : target.y()>=8)
              return;
	  if constexpr (T==ROOK || T==BISHOP || T==PAWN)
            return;
          if constexpr (T==LANCE)
            if (P==BLACK ? target.y()==2 : target.y()==8)
              return;
	}
	action.unknownMove(from,target,p1,T,false,P);
      }
      template<Player P,Ptype T>
      static void generatePiecePtype(const EffectState& state,Piece p, Square target, Piece p1,MoveStore& action)
      {
	if constexpr (T==KING){
	  assert(!state.hasEffectAt(alt(P),p.square()));
	  if(state.hasEffectAt(alt(P),target)) return;
	}
	else if(state.pin(P).test(p.id())){
	  Direction d=state.pinnedDir<P>(p);
	  Direction d1=base8_dir_unsafe<P>(p.square(),target);
	  if(primary(d)!=primary(d1)) return;
	}
	generatePiecePtypeUnsafe<P,T>(state,p,target,p1,action);
      }
      /**
       * pinの場合はそれに応じた手を生成する
       * @param T - moveTypeがTの駒
       * @param state - 手を作成する局面，手番はPと一致
       * @param p - 盤面上に存在するPの駒
       * @param action - 手生成用のMoveStore
       */
      template <Player P,Ptype T,bool useDirMask>
      static void generatePtype(const EffectState& state,Piece p, MoveStore& action,int dirMask=0);

      template <Player P,Ptype T>
      static void generatePtype(const EffectState& state,Piece p, MoveStore& action)
      {
	int dummy=0;
	generatePtype<P,T,false>(state,p,action,dummy);
      }
      /**
       * Generate moves without stating the Ptype as template param.
       * pinでないことが判明している時に呼び出す
       * @param T - moveTypeがTの駒
       * @param state - 手を作成する局面，手番はPと一致
       * @param p - 盤面上に存在するPの駒
       * @param action - 手生成用のMoveStore
       */
      template <Player P,Ptype T,bool useDirMask>
      static void generatePtypeUnsafe(const EffectState& state,Piece p, MoveStore& action,int dirMask);
      template <Player P,Ptype T>
      static void generatePtypeUnsafe(const EffectState& state,Piece p, MoveStore& action)
      {
	int dummy=0;
	generatePtypeUnsafe<P,T,false>(state,p,action,dummy);
      }

      /**
       * Generate moves without stating the Ptype as template param.
       * 自玉に王手がかかっていない時に呼ぶ．
       * @param state - 手を作成する局面，手番はPと一致
       * @param p - 盤面上に存在するPの駒
       * @param action - 手生成用のMoveStore
       */
      template <Player P,bool useDirMask>
      static void generate(const EffectState& state,Piece p, MoveStore& action,int dirMask=0);
    };

  } // namespace move_generator
} // namespace osl

namespace osl
{
  namespace move_generator
  {
    using osl::move_action::Store;
    /**
     * 逃げる手を生成
     * 生成される手はunique
     */
    class Escape
    {
    public:
      /**
       * 玉kingにfromにある駒から王手がかかってい
       * る時に，長い利きの途中に入る手を
       * 生成する(合駒，駒移動)．
       * breakThreatmateから直接呼ばれる．
       */
      template<Player P>
      static void generateBlockingKing(const EffectState& state,Piece king,Square from,Store &action);
      /**
       * 相手の駒を取ることによって利きを逃れる.
       * 逃げ出す駒で取る手は生成しない（2003/5/12）
       * @param target toru koma no pos
       */
      template<Player P>
      static void generateCaptureKing(const EffectState& state,Piece p,Square target,Store& action) {
	Capture::template escapeByCapture<P>(state,target,p,action);
      }

      /** 
       * @param p king
       */
      template<Player P>
      static void escape_king(const EffectState& state,Store& action);
    };
  } // namespace move_generator
  struct GenerateEscapeKing
  {
    /** 不成の受けも作成 */
    static void generate(const EffectState& state, MoveVector& out);
  };
  //using move_generator::GenerateEscape;
} // namespace osl
/* MINIOSL_MORE_H */
#endif
