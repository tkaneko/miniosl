#ifndef MINIOSL_HASH_H
#define MINIOSL_HASH_H
#include "state.h"
#include <memory>

namespace osl
{
  /**
   * Keep number of pieces in hand for a player in 32bit.
   *
   * provide efficient comparison as hasMoreThan 
   *
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
  class PieceStand {
    mutable uint32_t flags;
  public:
    explicit PieceStand(unsigned int value=0) : flags(value) {
    }
    explicit PieceStand(Player, const BaseState&);
    PieceStand(int pawnCount, int lanceCount, 
	       int knightCount, int silverCount,
	       int goldCount, int bishopCount,
	       int rookCount, int kingCount) 
      : flags(0) {
      add(PAWN, pawnCount);
      add(LANCE, lanceCount);
      add(KNIGHT, knightCount);
      add(SILVER, silverCount);
      add(GOLD, goldCount);
      add(BISHOP, bishopCount);
      add(ROOK, rookCount);
      add(KING, kingCount);
    }

    unsigned int get(Ptype type) const {
      return (flags >> (shift[idx(type)])) & mask[idx(type)];
    }
    /**
     * this と other が BLACK の持駒と考えた時に，
     * this の方が同じか沢山持っていれば真.
     */
    template <Player P>
    bool hasMoreThan(PieceStand other) const {
      if (P == BLACK)
	return isSuperiorOrEqualTo(other);
      else
	return other.isSuperiorOrEqualTo(*this);
    }
    bool hasMoreThan(Player P, PieceStand other) const {
      if (P == BLACK)
	return hasMoreThan<BLACK>(other);
      else
	return hasMoreThan<WHITE>(other);
    }

    void add(Ptype type, unsigned int num=1) {
      assert(is_basic(type));
      assert(num == (num & mask[idx(type)]));
      flags += (num << (shift[idx(type)]));
      assert(testCarries() == 0);	// overflow 検出
    }    
    void sub(Ptype type, unsigned int num=1) {
      assert(is_basic(type));
      assert(num == (num & mask[idx(type)]));
      assert(get(type) >= num);
      flags -= (num << (shift[idx(type)]));
    }

    std::string to_csa(Player color) const;

    /**
     * 加算可能なら加える.
     * 速度が必要なところでは使ってないので .cc に移動．
     */
    void tryAdd(Ptype type);
    bool canAdd(Ptype type) const;
    /**
     * 1枚以上持っていれば減らす
     */
    void trySub(Ptype type) {
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
    void addAtmostOnePiece(PieceStand const& ps) {
#ifndef NDEBUG
      const PieceStand copy(*this);
#endif
      assert(! ps.testCarries());
      assert(ps.atMostOneKind());
      flags += ps.to_uint();
      assert(carryUnchangedAfterAdd(copy, ps));
    }

    void subAtmostOnePiece(PieceStand const& ps) {
#ifndef NDEBUG
      const PieceStand copy(*this);
#endif
      assert(! ps.testCarries());
      assert(ps.atMostOneKind());
      flags -= ps.to_uint();
      assert(carryUnchangedAfterSub(copy, ps));
    }
  private:
    bool carryUnchangedAfterAdd(const PieceStand& original, const PieceStand& other) const;
    bool carryUnchangedAfterSub(const PieceStand& original, const PieceStand& other) const;
  public:
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
    unsigned int to_uint() const { return flags; }
    /** どれかの駒を一枚でも持っている */
    bool any() const { return flags; }
    /**
     * 種類毎に this と other の持駒の多い方を取る
     */
    PieceStand max(PieceStand other) const {
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
    PieceStand max2(PieceStand other) const {
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

    PieceStand nextStand(Player pl, Move move) const {
      assert(move.isNormal());
      PieceStand result = *this;
      if (move.player() == pl) {
	if (auto ptype = move.capturePtype(); ptype!=Ptype_EMPTY) {
	  result.add(unpromote(ptype));
	}
	else if (move.isDrop()) {
	  const Ptype ptype = move.ptype();
	  assert(get(ptype));
	  result.sub(ptype);
	}
      }
      return result;
    }
    PieceStand nextStand(Move move) const { return nextStand(move.player(), move); }
    PieceStand previousStand(Player pl, Move move) const {
      assert(move.isNormal());
      PieceStand result = *this;
      if (move.player() == pl) {
	if (Ptype ptype = move.capturePtype(); ptype!=Ptype_EMPTY) {
	  const Ptype before = unpromote(ptype);
	  assert(get(before));
	  result.sub(before);
	}
	else if (move.isDrop()) {
	  const Ptype ptype = move.ptype();
	  result.add(ptype);
	}
      }
      return result;
    }
    PieceStand previousStand(Move move) const { return previousStand(move.player(), move); }

    static constexpr uint32_t carryMask = 0x48822224;
  private:
    static const CArray<unsigned char,Ptype_MAX+1> shift;
    static const CArray<unsigned char,Ptype_MAX+1> mask;
  };

  inline bool operator==(PieceStand l, PieceStand r) {
    assert(! l.testCarries());
    assert(! r.testCarries());
    return l.to_uint() == r.to_uint();
  }
  inline bool operator!=(PieceStand l, PieceStand r) {
    return ! (l == r);
  }
  inline bool operator<(PieceStand l, PieceStand r) {
    assert(! l.testCarries());
    assert(! r.testCarries());
    return l.to_uint() < r.to_uint();
  }
  std::ostream& operator<<(std::ostream&, PieceStand l);

} // namespace osl

namespace osl
{
  extern const std::array<uint64_t, 81*32> hash_code_on_board_piece;
  /** zobrist hash for onboard pieces, including side to move, ignoring pieces in hand */
  uint64_t zobrist_hash_of_board(const BaseState&);

  struct HashSupplement {
    uint16_t black_king: 7 =0;
    uint16_t turn: 1 =0;
    uint16_t white_king: 7 =0;
    uint16_t in_check: 1 =0;

    friend inline bool operator==(const HashSupplement&, const HashSupplement&) = default;
    friend inline bool operator!=(const HashSupplement&, const HashSupplement&) = default;
  };
  struct HistoryStatus {
    uint16_t prev_dist: 8 =0; // div2
    uint16_t count: 3 =0;
    uint16_t reserved: 5=0;

    friend inline bool operator==(const HistoryStatus&, const HistoryStatus&) = default;
    friend inline bool operator!=(const HistoryStatus&, const HistoryStatus&) = default;
  };
  /** 128bit data to detect repetition in a game history.
   *  64 + 32 + 16bit for state,
   *  16bit for history */
  struct HashStatus {
    uint64_t board_hash =0;
    PieceStand black_stand;
    HashSupplement supp;
    HistoryStatus history;

    HashStatus() {}
    HashStatus(const BaseState& state, bool in_check)
      : board_hash(zobrist_hash_of_board(state)), black_stand(BLACK, state) {
      supp = supplementary_info(state, in_check);
    }
    HashStatus(const EffectState& state) : HashStatus(state, state.inCheck()) {} // delegate

    HashStatus zero_history() const {
      HashStatus copy = *this;
      copy.history = HistoryStatus();
      return copy;
    }
    
    HashStatus new_zero_history(Move moved, bool in_check) const;
  
    bool is_repeat_of(const HashStatus& rhs) const {
      return zero_history() == rhs.zero_history();
    }
    int repeat_count() const { return history.count; }
    bool has_repeat_state() const { return history.count > 0; }
    int distance_to_previous_repeat() const { return history.prev_dist*2; }
    Player turn() const { return players[supp.turn]; }
    bool in_check() const { return supp.in_check; }
    Square king(Player pl) const {
      if (pl == BLACK) return Square::from_index81(supp.black_king);
      return Square::from_index81(supp.white_king);
    }
    
    static uint64_t code(Square sq, PtypeO ptypeo) {
      return hash_code_on_board_piece[sq.index81()*32+idx(ptypeo)];
    }
    static HashSupplement supplementary_info(const BaseState& state, bool in_check) {
      HashSupplement supp;
      supp.black_king = state.kingSquare(BLACK).index81();
      supp.turn = idx(state.turn());
      supp.white_king = state.kingSquare(WHITE).index81();
      supp.in_check = in_check;
      return supp;
    }
    friend inline bool operator==(const HashStatus&, const HashStatus&) = default;
    friend inline bool operator!=(const HashStatus&, const HashStatus&) = default;
  };
  std::ostream& operator<<(std::ostream& os, const HashStatus& hash);

  int consecutive_in_check(const std::vector<HashStatus>& history, int id);
  
  class HistoryTable {
  public:
    HistoryTable();
    ~HistoryTable();
    GameResult add(int move_number, HashStatus& now, const std::vector<HashStatus>& history);

    typedef std::vector<std::pair<PieceStand,int>> vector_t;
    /** @return -1 not found */
    static int latest_same_state(const vector_t&, const HashStatus&);
    static int first_same_state(const vector_t&, const HashStatus&);
  private:
    class Table;
    std::shared_ptr<Table> table;
  };
  
}


#endif
// MINIOSL_HASH_H
