#ifndef MINIOSL_BASIC_TYPE_H
#define MINIOSL_BASIC_TYPE_H

#include <array>
#include <cstdint>
#include <iosfwd>
#include <bit>
#include <ranges>
#include <numeric>
#include <cassert>

namespace osl {
  typedef uint64_t mask_t;
  constexpr inline mask_t lowest_bit(mask_t bs) {
    return mask_t{bs & (-bs)};
  }
  constexpr inline int take_one_bit(mask_t& bs) {
    auto n = std::countr_zero(bs);
    bs &= bs-1;
    return n;
  }
  constexpr inline int has_multiple_bit(mask_t bs) { return (bs != 0) && (bs &= bs-1) != 0; }
  constexpr mask_t one_hot(int num) { return 1uLL << num; }
  template <class IntLike> constexpr inline bool any(IntLike value) { return value; }  
  template <class IntLike> constexpr inline bool none(IntLike value) { return value==0; }  
  template <class IntLike, class SmallIntLike> constexpr
  inline bool bittest(IntLike value, SmallIntLike n) { return any(value & one_hot(n)); }
  
  enum class Player {
    BLACK=0, WHITE= -1
  };
  using enum Player;
  constexpr Player players[2] = { BLACK, WHITE };
  constexpr int Int(Player pl) { return static_cast<int>(pl); }
  
  constexpr Player alt(Player player) {
    return Player{-1-Int(player)};
  }
  constexpr int idx(Player player) { return -Int(player); }
  constexpr int sign(Player player) { return 1+(Int(player)<<1); } // +1 or -1
  constexpr int mask(Player player) { return Int(player); }
  constexpr bool is_valid(Player player) { return player == BLACK || player == WHITE; }
  std::ostream& operator<<(std::ostream& os, Player);

  /** 駒の種類の4ビット表現 */
  enum class Ptype {
    Ptype_EMPTY=0, Ptype_EDGE=1,
    PPAWN=2, PLANCE=3, PKNIGHT=4, PSILVER=5, PBISHOP=6, PROOK=7,
    KING=8, GOLD=9, PAWN=10, LANCE=11, KNIGHT=12, SILVER=13, BISHOP=14, ROOK=15,
  };
  using enum Ptype;
  constexpr Ptype all_ptype[] = { Ptype_EMPTY, Ptype_EDGE,
    PPAWN, PLANCE, PKNIGHT, PSILVER, PBISHOP, PROOK,
      KING, GOLD, PAWN, LANCE, KNIGHT, SILVER, BISHOP, ROOK,
  };
  constexpr Ptype piece_ptype[] = { 
    PPAWN, PLANCE, PKNIGHT, PSILVER, PBISHOP, PROOK,
      KING, GOLD, PAWN, LANCE, KNIGHT, SILVER, BISHOP, ROOK,
  };
  constexpr Ptype basic_ptype[] = { 
    KING, GOLD, PAWN, LANCE, KNIGHT, SILVER, BISHOP, ROOK,
  };
  /** 持駒の表示で良く使われる順番 */
  constexpr std::array<Ptype,7> piece_stand_order = { ROOK, BISHOP, GOLD, SILVER, KNIGHT, LANCE, PAWN, };

  constexpr int Int(Ptype ptype) { return static_cast<int>(ptype); }
  constexpr int Ptype_MIN=0, Ptype_Basic_MIN=Int(KING),
    Ptype_Piece_MIN=2, Ptype_MAX=15, Ptype_SIZE=16;
  constexpr int idx(Ptype ptype) { return Int(ptype); }
  constexpr mask_t one_hot(Ptype ptype) { return one_hot(idx(ptype)); }
  constexpr int basic_idx(Ptype ptype) { return Int(ptype) - Ptype_Basic_MIN; }
  constexpr bool is_valid(Ptype ptype) {
    return Int(ptype)>=Ptype_MIN && Int(ptype)<=Ptype_MAX;
  }
  constexpr auto all_ptype_range() {
    return std::views::iota(Ptype_MIN, Ptype_MAX+1)
      | std::views::transform([](int n) { return Ptype(n); });
  }
  constexpr auto all_piece_ptype() {
    return std::views::iota(Ptype_Piece_MIN, Ptype_MAX+1)
      | std::views::transform([](int n) { return Ptype(n); });
  }
  constexpr auto all_basic_ptype() {
    return std::views::iota(Ptype_Basic_MIN, Ptype_MAX+1)
      | std::views::transform([](int n) { return Ptype(n); });
  }
  
  std::istream& operator>>(std::istream& is, Ptype& ptype);
  std::ostream& operator<<(std::ostream& os,const Ptype ptype);
  
  /**
   * ptypeが普通のコマ (空白やEDGEでない)
   */
  constexpr bool is_piece(Ptype ptype) { return Int(ptype) >= Ptype_Piece_MIN; }
  /**
   * ptypeが基本型 (promoteしていない)
   */
  constexpr bool is_basic(Ptype ptype) { return Int(ptype) > Int(PROOK); }
  constexpr bool is_promoted(Ptype ptype) { return Int(ptype) < Int(KING);  }

  /**
   * ptypeがpromote可能な型 (promote済みの場合はfalse)
   */
  constexpr bool can_promote(Ptype ptype) { return Int(ptype) > Int(GOLD);  }
  
  /** 
   * promote前の型.  promoteしていない型の時はそのまま
   */
  constexpr Ptype unpromote(Ptype ptype) {
    return (! is_piece(ptype)) ? ptype : Ptype{Int(ptype)|8}; 
  }
  
  constexpr Ptype promote(Ptype ptype) {
    return can_promote(ptype) ? Ptype{Int(ptype)-8} : ptype;
  }

  constexpr bool is_major_basic(Ptype ptype) { return Int(ptype) >= 14; }
  constexpr bool is_major(Ptype ptype) { return (Int(ptype)|8)>=14; }
  
  /**
   * Player + Ptype [-15, 15] 
   * PtypeO の O は Owner の O
   */
  enum PtypeO {
    Ptypeo_MIN_sentinel = Int(Ptype_EMPTY)-16,
    Ptypeo_MAX_sentinel = 15,
  };
  constexpr int Int(PtypeO ptypeO) { return static_cast<int>(ptypeO); }
  constexpr PtypeO newPtypeO(Player player, Ptype ptype) {
    return PtypeO(Int(ptype)-(16&Int(player)));
  }

  constexpr PtypeO Ptypeo_EMPTY = newPtypeO(BLACK,Ptype_EMPTY);
  constexpr PtypeO Ptypeo_EDGE  = newPtypeO(WHITE,Ptype_EDGE);
  constexpr int Ptypeo_MIN = Int(PtypeO::Ptypeo_MIN_sentinel);
  constexpr int Ptypeo_MAX = Int(PtypeO::Ptypeo_MAX_sentinel);
  constexpr int Ptypeo_SIZE= Ptypeo_MAX-Ptypeo_MIN+1;

  constexpr unsigned int idx(PtypeO ptypeo) { return Int(ptypeo) - Ptypeo_MIN; }
  constexpr unsigned int one_hot(PtypeO ptypeo) { return 1u << idx(ptypeo); }
  constexpr Ptype ptype(PtypeO ptypeO) { return Ptype{Int(ptypeO) & 15}; }
  constexpr bool is_valid(PtypeO ptypeO) {
    return (Int(ptypeO) >= Ptypeo_MIN) && (Int(ptypeO) <= Ptypeo_MAX);
  }
  constexpr auto all_ptypeO_range() {
    return std::views::iota(Ptypeo_MIN, Ptypeo_MAX+1)
      | std::views::transform([](int n) { return PtypeO(n); });
  }
  
  /** pieceをpromoteさせる. */
  constexpr PtypeO promote(PtypeO ptypeO) {
    return can_promote(ptype(ptypeO)) ? PtypeO(Int(ptypeO)-8) : ptypeO;
  }
  
  /** pieceをunpromoteさせる.  promoteしていないptypeを与えてもよい */
  constexpr PtypeO unpromote(PtypeO ptypeO) { return PtypeO(Int(ptypeO)|8); }
  
  /**
   * EMPTY, EDGEではない
   */
  constexpr bool is_piece(PtypeO ptypeO) { return is_piece(ptype(ptypeO)); }
  constexpr Player owner(PtypeO ptypeO) { return Player{Int(ptypeO)>>31}; }

  /** unpromoteすると共に，ownerを反転する． */
  constexpr PtypeO captured(PtypeO ptypeO) {
    return is_piece(ptypeO) ? PtypeO((Int(ptypeO)|8)^(~15)) : ptypeO;
  }
  /** owner を反転する */
  constexpr PtypeO alt(PtypeO ptypeO) {
    int v=Int(ptypeO);
    return PtypeO(v^((1-(v&15))&~15));
  }
  constexpr bool can_promote(PtypeO ptypeO) { return can_promote(ptype(ptypeO)); }
  /**
   * ptypeOが promote済みかどうか
   */
  constexpr bool is_promoted(PtypeO ptypeO) { return is_promoted(ptype(ptypeO)); }
  
  std::ostream& operator<<(std::ostream& os,const PtypeO ptypeO);

  /** Direction.
   * steps achievable by a legal move only, i.e., no DDL or DDR.
   * still, subtraction (sq - to_offset(UUL)) yields inverse step.
   */
  enum class Direction{
    UL=0, U=1, UR=2,
    L=3, R=4,
    DL=5, D=6, DR=7,
    UUL=8, UUR=9,
    Long_UL=10, Long_U=11, Long_UR=12,
    Long_L=13, Long_R=14,
    Long_DL=15, Long_D=16, Long_DR=17,
  };
  constexpr int 
    Direction_MIN=0, Base_Direction_MIN=0, Base8_Direction_MIN=0,
    Base8_Direction_MAX=7, Base_Direction_MAX=9, Base_Direction_SIZE=10,
    Long_Direction_MIN=10, Long_Direction_MAX=17,
    Direction_MAX=17, Direction_INVALID_VALUE=18, Direction_SIZE=18;
  using enum Direction;
  constexpr int Int(Direction d) { return static_cast<int>(d); }
  constexpr auto all_directions() {
    return std::views::iota(Direction_MIN, Direction_SIZE)
      | std::views::transform([](int n) { return Direction(n); });
  }
  constexpr auto base8_directions() {
    return std::views::iota(Base8_Direction_MIN, Base8_Direction_MAX+1)
      | std::views::transform([](int n) { return Direction(n); });
  }
  constexpr auto long_directions() {
    return std::views::iota(Long_Direction_MIN, Long_Direction_MAX+1)
      | std::views::transform([](int n) { return Direction(n); });
  }
  constexpr auto knight_directions = {UUL, UUR};
  
  constexpr bool is_basic(Direction d) { return Int(d) <= Base_Direction_MAX; }
  constexpr bool is_base8(Direction d) { return Int(d) <= Base8_Direction_MAX; }
  constexpr bool is_long(Direction d) { return Int(d) >= Long_Direction_MIN; }
  constexpr Direction inverse(Direction d) {
    return is_base8(d)
      ? Direction{7 - Int(d)}
      : (is_long(d) ? Direction(27 - Int(d)) : d);
  }
  constexpr bool is_forward(Direction d) {
    return d == UL || d == U || d == UR || d == UUL || d == UUR
      || d == Long_UL || d == Long_U || d == Long_UR;
  }

  /**
   * 8方向について，primitiveな4方向を求める
   * dとしてknight, INVALIDなども来る
   */
  constexpr Direction primary(Direction d) {
    return (Int(d)<4 || (is_long(d) && Int(d)<14)) ? d : inverse(d);
  }
  constexpr int idx(Direction d) { return Int(d); }  
  constexpr int base8_idx(Direction d) { return Int(d)-Int(Long_UL); }  
  constexpr Direction long_to_base8(Direction d) { return Direction{base8_idx(d)}; }  
  constexpr Direction to_long(Direction d) { 
    return is_base8(d) ? Direction{Int(d) + Int(Long_UL)} : d;
  }
  constexpr int one_hot(Direction dir) { return (1<<Int(dir)); }
  constexpr bool is_valid(Direction d) { return Direction_MIN<=Int(d) && Int(d)<=Direction_MAX; }
  
  std::ostream& operator<<(std::ostream& os,const Direction d);
}
namespace osl
{
  constexpr std::array<const char *, Ptype_SIZE> ptype_csa_names = {{
      "..",  "XX",
      "TO", "NY", "NK", "NG", "UM", "RY",
      "OU", "KI", "FU", "KY", "KE", "GI", "KA", "HI",
    }};
  constexpr std::array<const char *, Ptype_SIZE> ptype_en_names = {{
      "Ptype_EMPTY", "Ptype_EDGE",
      "PPAWN", "PLANCE", "PKNIGHT", "PSILVER", "PBISHOP", "PROOK",
      "KING",  "GOLD", "PAWN", "LANCE", "KNIGHT", "SILVER", "BISHOP", "ROOK",
    }};
  /** 打ち歩詰以外で promoteが常に得 */
  constexpr std::array<bool, Ptype_SIZE> ptype_prefer_promote = {{
      false, false,
      true, false, false, false, true, true,
      false, false, true, false, false, false, true, true,
    }};
  constexpr std::array<std::pair<int,int>, Ptype_SIZE> ptype_drop_range = {{
      {0, 0}, {0, 0},
      {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, 
      {1, 1}, {1, 9}, {2, 9}, {2, 9}, {3, 9}, {1, 9}, {1, 9}, {1, 9}, 
    }};
  constexpr std::array<std::pair<int,int>, Ptype_SIZE> ptype_drop_range_white = {{
      {0, 0}, {0, 0},
      {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, 
      {9, 9}, {9, 1}, {8, 1}, {8, 1}, {7, 1}, {9, 1}, {9, 1}, {9, 1}, 
    }};
  constexpr int set(Direction l, Direction r)  { return one_hot(l) | one_hot(r); }
  constexpr int set(Direction l, Direction m, Direction r)  { return one_hot(l) | one_hot(m) | one_hot(r); }
  constexpr int ptype_gold_move = set(UL, U, UR) | set(L, R, D);
  constexpr int ptype_king_move = ptype_gold_move | set(DL, DR);
  constexpr int ptype_bishop_move = set(Long_UL, Long_UR)|set(Long_DL, Long_DR);
  constexpr int ptype_rook_move = set(Long_U, Long_L)|set(Long_R, Long_D);

  constexpr std::array<int, Ptype_SIZE> ptype_move_direction = {{
      0, 0,
      ptype_gold_move, ptype_gold_move, ptype_gold_move, ptype_gold_move,
      ptype_bishop_move | set(U, D) | set(L, R), ptype_rook_move |set(UL, UR) | set(DL, DR),
      ptype_king_move, ptype_gold_move, one_hot(U), one_hot(Long_U), set(UUL, UUR), (set(UL, U, UR) | set(DL, DR)),
      ptype_bishop_move, ptype_rook_move,
    }};
  constexpr std::array<Ptype, Ptype_SIZE> ptype_move_type = {{
      Ptype_EMPTY, Ptype_EDGE,
      GOLD, GOLD, GOLD, GOLD, PBISHOP, PROOK,
      KING, GOLD, PAWN, LANCE, KNIGHT, SILVER, BISHOP, ROOK,
    }};
  constexpr std::array<std::pair<int,int>, Ptype_SIZE> ptype_piece_id = {{
      {0, 0}, {0, 0},
      {0, 18}, {26, 30}, {18, 22}, {22, 26}, {36, 38}, {38, 40}, 
      {30, 32}, {26, 30}, {0, 18}, {32, 36}, {18, 22}, {22, 26}, {36, 38}, {38, 40}, 
    }};
  constexpr int ptype_piece_count(Ptype ptype) {
    auto r = ptype_piece_id[idx(ptype)];
    return r.second-r.first;
  }
  
  template <Direction d>
  constexpr int ptype_set() {
    auto good = std::views::iota(Ptype_Piece_MIN, Ptype_MAX+1) // all_piece_ptype()
      | std::views::filter([](int p){
        return bittest(ptype_move_direction[p], d) || bittest(ptype_move_direction[p], to_long(d)); })
      | std::views::transform([](int p){ return one_hot(p); });
    return std::reduce(good.begin(), good.end()); // fold@c++23
  }
    
  constexpr int king_piece_id(Player P) { return ptype_piece_id[idx(KING)].first+idx(P); }
  constexpr bool ptype_has_long_move(Ptype T) { return ptype_piece_id[idx(T)].first >=32; }
  constexpr bool is_valid_piece_id(int n) { return 0 <= n && n < 40; }
  constexpr bool is_long_piece_id(int n) { return 32 <= n && n < 40; }
  constexpr auto to_range(Ptype ptype) {
    auto [l, r] = ptype_piece_id[idx(ptype)];
    return std::views::iota(l, r);
  }
  constexpr auto all_piece_id() { return std::views::iota(0, 40); }
  constexpr auto long_piece_id() { return std::views::iota(32, 40); }

  constexpr int long_piece_idx(int piece_id) { return piece_id - 32; }
}
namespace osl
{
  /**
   * 座標.
   *        盤面のインデックス
   * X, Yも1-9の範囲で表す 
   * Xは右から数える．Yは上から数える
   * なお駒台は0
   * <pre>
   * (A0)  ......................... (00)
   * (A1)  ......................... (01)
   * (A2) 92 82 72 62 52 42 32 22 12 (02)
   * (A3) 93 83 73 63 53 43 33 23 13 (03)
   * (A4) 94 84 74 64 54 44 34 24 14 (04)
   * (A5) 95 85 75 65 55 45 35 25 15 (05)
   * (A6) 96 86 76 66 56 46 36 26 16 (06)
   * (A7) 97 87 77 67 57 47 37 27 17 (07)
   * (A8) 98 88 78 68 58 48 38 28 18 (08)
   * (A9) 99 89 79 69 59 49 39 29 19 (09)
   * (AA) 9A 8A 7A 6A 5A 4A 3A 2A 1A (0A)
   * (AB) ...........................(0B)
   * (AC) ...........................(0C)
   * (AD) ...........................(0D)
   * (AE) ...........................(0E)
   * (AF) ...........................(0F) 
   * </pre>
   */
  /**
   * 座標の差分
   */
  enum class Offset {
    Offset_MIN=-0x100,
    Offset_ZERO=0,
    Offset_MAX=0x100,
  };
  using enum Offset;
  constexpr int OnBoard_Offset_MIN=-0x88, OnBoard_Offset_MAX=0x88, OnBoard_Offset_SIZE=0x88*2+1;
  constexpr int BOARD_HEIGHT=16;
  constexpr int Int(Offset offset) { return static_cast<int>(offset); }
  
  constexpr inline Offset make_offset(int dx, int dy) { return Offset{dx*BOARD_HEIGHT + dy}; }
  constexpr inline unsigned int idx(Offset o) { return Int(o) - Int(Offset_MIN); }
  constexpr inline unsigned int onboard_idx(Offset o) { return Int(o) - OnBoard_Offset_MIN; }
  constexpr inline Offset operator+(Offset l, Offset r) { return Offset{Int(l)+Int(r)}; }
  constexpr inline Offset operator-(Offset l, Offset r) { return Offset{Int(l)-Int(r)}; }
  constexpr inline Offset operator-(Offset l) { return Offset{-Int(l)}; }
  constexpr inline Offset operator*(Offset l, int r) { return Offset{Int(l)*r}; }
  constexpr inline bool operator<(Offset l, Offset r) { return Int(l) < Int(r); }
  // constexpr Offset inline change_view(Offset o, Player P) { return (P==BLACK) ? o : -o; }
  constexpr inline int change_y_view(Player P, int y) { return P==BLACK ? y : 10-y; }  

  std::ostream& operator<<(std::ostream&, Offset);
}
// altDir = inverse
namespace osl
{
  // UL, U, UR, L, R, DL, D, DR, UUL, UUR, Long_UL, Long_U, Long_UR, Long_L, Long_R, Long_DL, Long_D, Long_DR,
  struct DirectionTrait {
    int dx, dy;
  };
  constexpr DirectionTrait direction_trait[] = {
    /* UL */ { 1, -1 }, /* U */ { 0, -1 }, /* UR */ { -1, -1},
    /* L */ { 1, 0 }, /* R */ { -1, 0 },
    /* DL */ { 1, 1 }, /* D */ { 0, 1 }, /* DR */ { -1, 1 },
    /* UUL */ { 1, -2 }, /* UUR */ { -1, -2 },
    /* Long_UL */ { 1, -1 }, /* Long_U */ { 0, -1 }, /* Long_UR */ { -1, -1},
    /* Long_L */ { 1, 0 }, /* Long_R */ { -1, 0 },
    /* Long_DL */ { 1, 1 }, /* Long_D */ { 0, 1 }, /* Long_DR */ { -1, 1 },
  };
  constexpr int black_dx(Direction d) { return direction_trait[Int(d)].dx; }
  constexpr int black_dy(Direction d) { return direction_trait[Int(d)].dy; }
  
  constexpr Offset black_offset(Direction dir) { return make_offset(black_dx(dir), black_dy(dir)); }
  /** switch between player-view and black-view */
  constexpr Direction change_view(Player P, Direction dir) {
    return (P == BLACK) ? dir : inverse(dir);
  }  
  constexpr std::array<Offset,Direction_SIZE> direction_offsets = {{
    black_offset(UL), black_offset(U), black_offset(UR),
    black_offset(L), black_offset(R),
    black_offset(DL), black_offset(D), black_offset(DR),
    black_offset(UUL), black_offset(UUR),
    black_offset(Long_UL), black_offset(Long_U), black_offset(Long_UR),
    black_offset(Long_L), black_offset(Long_R),
    black_offset(Long_DL),black_offset(Long_D),  black_offset(Long_DR),
  } };
  template<Player P>
  constexpr inline Offset to_offset(Direction dir) { return direction_offsets[idx(dir)]*sign(P); }
  constexpr inline Offset to_offset(Player pl, Direction dir) {
    return (pl==BLACK) ? to_offset<BLACK>(dir) : to_offset<WHITE>(dir);
  }
} // namespace osl
namespace osl
{
  constexpr auto board_y_range() { // 1,2,...,9
    return std::views::iota(1, 10);
  }
  constexpr auto board_x_range() { // 9,8,...,1
    return std::views::iota(1, 10) | std::views::reverse;
  }
  constexpr bool promote_area_y(Player P, int y) { return P == BLACK ? y <= 3 : y >= 7; }
  class Square
  {
    unsigned int square;
    explicit Square(int p) : square(p)
    {
    }
  public:
    static const Square makeDirect(int value) { return Square(value); }
    unsigned int uintValue() const { return square; }
    enum {
      Piece_STAND=0,
      MIN=0,
      SIZE=0x100
    };
    Square() : square(Piece_STAND) {
    }
    static const Square STAND() { return Square(Piece_STAND); }
    Square(int x, int y) : square((x*BOARD_HEIGHT)+y+1) {
    }
    static const Square nth(unsigned int i) { return Square(i+MIN); }
    /**
     * 将棋としてのX座標を返す. 
     */
    int x() const { return square >> 4; }
    /**
     * 将棋としてのY座標を返す. 
     */
    int y() const { return (square&0xf)-1; }
    /**
     * y+1を返す
     */
    int y1() const { return square&0xf; }
    unsigned int index() const { assert(square < SIZE); return square - MIN; }
    static unsigned int indexMax() { return SIZE - MIN; }
    int indexForOffset32() const { return square + (square&0xf0); }

    bool isPieceStand() const { return square == Piece_STAND; }
    /**
     * 盤面上を表すかどうかの判定．
     * 1<=x() && x()<=9 && 1<=y() && y()<=9
     * Squareの内部表現に依存する．
     */
    bool isOnBoard() const { 
      return (0xffffff88&(square-0x12)&
	      ((unsigned int)((square&0x77)^0x12)+0xffffff77))==0;
    }
    /**
     * onBoardから8近傍のオフセットを足した点がedgeかどうかの判定
     * そこそこ速くなった．
     */
    bool isEdge() const { 
      assert(!isPieceStand() && 0<=x() && x()<=10 && 0<=y() && y()<=10);
      return (0x88&(square-0x12)&((square&0x11)+0xf7))!=0;
    }
    bool isValid() const;

    /** 
     * 後手の場合は盤面を引っくり返す.
     */
    Square blackView(Player player) const {
      assert(! isPieceStand());
      return (player == BLACK)
	? *this
	: makeDirect(Square(9,9).uintValue()+Square(1,1).uintValue()-uintValue());
    }
    Square rotate180() const {
      if (isPieceStand())
	return *this;
      Square ret=makeDirect(Square(9,9).uintValue()+Square(1,1).uintValue()-uintValue());
      return ret;
    }

    bool isPromoteArea(Player player) const { // promoteArea
      if (player==BLACK) 
	return (uintValue()&0xf)<=4;
      else 
	return (uintValue()&0x8)!=0;
    }
    constexpr static int index81(int x, int y) { return (y-1)*9+x-1; }
    int index81() const { return index81(x(), y()); }
    static Square from_index81(int n) { return Square(n % 9 + 1, n / 9 + 1); }
  public:
    /**
     * 2つのSquare(onBoardであることが前提)が，
     * xが等しいかyが等しい
     */
    bool isULRD(Square sq) const {
      assert(isOnBoard() && sq.isOnBoard());
      unsigned int v=uintValue() ^ sq.uintValue();
      return (((v+0xefull)^v)&0x110ull)!=0x110ull;
    }
    /**
     * 2つのSquare(onBoardであることが前提)のxが等しい
     */
    bool isUD(Square sq) const {
      assert(isOnBoard() && sq.isOnBoard());
      unsigned int v=uintValue() ^ sq.uintValue();
      return (v&0xf0)==0;
    }
    /**
     * sqがPlayer Pにとって上
     */
    bool isU(Player P, Square sq) const {
      assert(isOnBoard() && sq.isOnBoard());
      unsigned int v=uintValue() ^ sq.uintValue();
      if(P==BLACK)
	return ((v|(uintValue()-sq.uintValue()))&0xf0)==0;
      else
	return ((v|(sq.uintValue()-uintValue()))&0xf0)==0;
    }
    /**
     * 2つのSquare(onBoardであることが前提)のyが等しい
     */
    bool isLR(Square sq) const {
      assert(isOnBoard() && sq.isOnBoard());
      unsigned int v=uintValue() ^ sq.uintValue();
      return (v&0xf)==0;
    }
    Square& operator+=(Offset offset) {
      square += Int(offset);
      return *this;
    }
    Square& operator-=(Offset offset) {
      square -= Int(offset);
      return *this;
    }
    const Square operator+(Offset offset) const {
      Square result(*this);
      return result+=offset;
    }
    const Square operator-(Offset offset) const {
      Square result(*this);
      return result-=offset;
    }
    const Offset operator-(Square other) const {
      return Offset(square - other.square);
    }
    template<int Y>
    bool yEq() {
      return (uintValue()&0xf)==(Y+1);
    }
    template<int Y>
    bool yLe() const {
      if constexpr (Y == 2)
        return (uintValue()&0xc)==0;
      else
        return (uintValue()&0xf)<=(Y+1);
    }
    template<int Y>
    bool yGe() const {
      if constexpr (Y==7)
        return (uintValue()&0x8)!=0;
      else
        return (uintValue()&0xf)>=(Y+1);
    }
    friend inline bool operator==(Square, Square) = default;
    friend inline bool operator!=(Square, Square) = default;
  };

  inline bool operator<(Square l, Square r) { return l.uintValue() < r.uintValue(); }
  inline bool operator>(Square l, Square r) { return l.uintValue() > r.uintValue(); }
  std::ostream& operator<<(std::ostream&, Square);

  // extended piece id (too large for PieceMask)
  constexpr int Piece_ID_EMPTY=0x80;
  constexpr int Piece_ID_EDGE=0x40;
  /**
   * 駒.
   * 駒はptypeo(-15 - 15), 番号(0-39), Square (0-0xff)からなる 
   * 上位16 bitでptypeo, 8bitで番号, 8bitで Square とする．
   * 空きマスは 黒，Ptype_EMPTY, 番号 0x80, Square 0
   * 盤外は     白，Ptype_EDGE,  番号 0x40, Square 0
   */
  class Piece
  {
    int packed;
    Piece(int p) : packed(p)
    {
    }
  public:
    static constexpr int SIZE=40;
    static const Piece makeDirect(int value) { return Piece(value); }
    int intValue() const { return packed; }
    static const Piece EMPTY()  { return Piece(BLACK,Ptype_EMPTY,Piece_ID_EMPTY,Square::STAND()); }
    static const Piece EDGE() { return Piece(WHITE,Ptype_EDGE,Piece_ID_EDGE,Square::STAND()); }
    static constexpr int BitOffsetPtype=16;
    static constexpr int BitOffsetPromote=BitOffsetPtype+3;
    static constexpr int BitOffsetMovePromote=BitOffsetPromote+4;
    
    Piece(Player owner, Ptype ptype, int num, Square square)
      : packed((Int(owner)<<20)
	      +(Int(ptype)<<BitOffsetPtype)
	      +((num)<<8)+ square.uintValue())
    {
    }
    Piece() : packed(EMPTY().packed)
    {
    }
    Ptype ptype() const {
      return Ptype{(packed>>BitOffsetPtype)&0xf};
    }
    PtypeO ptypeO() const {
      return PtypeO(packed>>BitOffsetPtype);
    }

    int id() const { return ((packed&0xff00)>>8); }

    const Square square() const {
      return Square::makeDirect(packed&0xff);
    }
    void setSquare(Square square) {
      packed = (packed&0xffffff00)+square.uintValue();
    }
  public:
    /**
     * piece がプレイヤーPの持ち物でかつボード上にある駒の場合は true.
     * 敵の駒だったり，駒台の駒だったり，Piece::EMPTY(), Piece_EDGEの場合は false
     * @param P(template) - プレイヤー
     * @param piece - 
     */
    template<Player P>
    bool isOnBoardByOwner() const { return isOnBoardByOwner(P); }
    /**
     * isOnBoardByOwner の通常関数のバージョン.
     */
    bool isOnBoardByOwner(Player owner) const
    {
      if(owner==BLACK)
	return static_cast<int>(static_cast<unsigned int>(packed)&0x800000ff)>0;
      else
	return static_cast<int>((-packed)&0x800000ff)>0;
    }

    /* 成る.  PROMOTE不可なpieceに適用不可 */
    const Piece promote() const {
      assert(can_promote(ptype()));
      return Piece(packed-0x80000);
    }

    /* 成りを戻す.  PROMOTE不可なpieceに適用可  */
    const Piece unpromote() const {
      return Piece((int)packed|0x80000);
    }

    /**
     * 取られたpieceを作成. unpromoteして，Squareは0に
     * 相手の持ちものにする
     */
    const Piece captured() const {
      // return (Piece)((((int)packed|0x80000)&0xffffff00)^0xfff00000);
      // をoptimizeする
      return Piece((packed&0xfff7ff00)^0xfff80000);
    }
    Piece drop(Square to) const {
      assert(! isOnBoard());
      return Piece(packed + Int(to-Square::STAND()));
    }
    /**
     * @param diff Offset(to-from) from がint で手に入るところで呼ぶので、呼び出し元で計算する設計
     */
    Piece move(Offset diff, int promote_mask) const {
      assert(! (isPromoted() && promote_mask));
      assert(promote_mask==0 || promote_mask==(1<<23));
      int promote = -(promote_mask>>(BitOffsetMovePromote-BitOffsetPromote));
      return Piece(packed + Int(diff) + promote);
    }
    /**
     * promoteした駒かどうかをチェックする
     */
    bool isPromoted() const { return (packed&(1<<19))==0; }

    bool isEmpty() const { return (packed&0x8000)!=0; }
    static constexpr bool isEmptyNum(int num) { return (num&0x80)!=0; }
    bool isEdge() const { return (packed&0x4000)!=0; }
    static bool isEdgeNum(int num) {
      assert(!isEmptyNum(num));
      return (num&0x40)!=0;
    }
    static bool isPieceNum(int num) { return (num&0xc0)==0; }
    bool isPiece() const {
      return (packed&0xc000)==0;
    }
    Player owner() const {
      assert(isPiece());
      return Player{packed>>20};
    }
    bool isOnBoard() const {
      assert(square().isValid());
      return ! square().isPieceStand();
    }

    /** Player Pの駒が，thisの上に移動できるか?
     * Piece_EMPTY 0x00008000
     * BLACK_PIECE 0x000XxxYY X>=2, YY>0
     * Piece_EDGE  0xfff14000
     * WHITE_PIECE 0xfffXxxYY X>=2, YY>0
     * @return thisが相手の駒かEMPTYならtrue
     * @param P 手番
     */
    template<Player P>
    bool canMoveOn() const {
      if constexpr (P == BLACK)
        return ((packed+0xe0000)&0x104000)==0;
      else
        return packed>=0;
    }
    bool canMoveOn(Player pl) const { return pl == BLACK ? canMoveOn<BLACK>() : packed>=0; }
    /**
     * pieceである前提で，更にBlackかどうかをチェックする．
     */
    bool pieceIsBlack() const {
      assert(isPiece());
      return static_cast<int>(packed)>=0;
    }
    friend inline bool operator==(Piece, Piece) = default;
    friend inline bool operator!=(Piece, Piece) = default;
  };

  inline bool operator<(Piece l, Piece r) { return l.intValue() < r.intValue(); }
  std::ostream& operator<<(std::ostream& os,const Piece piece);
}

namespace osl
{
  /**
   * 圧縮していない moveの表現.
   * special moves
   * - invalid: isNormal 以外の演算はできない
   * - declare_win: isNormal 以外の演算はできない
   * - pass: from, to, ptype, oldPtype はとれる．player()はとれない．
   * 
   * Pieceとpromotepをそろえる  -> 変える． 
   * 下位から 
   * 2009/12/10から
   * - to       : 8 bit 
   * - from     : 8 bit 
   * - capture ptype    : 4 bit 
   * - dummy    : 3 bit 
   * - promote? : 1 bit  
   * - ptype    : 4 bit --- promote moveの場合はpromote後のもの
   * - owner    : signed 
   */
  class Move
  {
  public:
    static constexpr int BitOffsetPromote=Piece::BitOffsetMovePromote;  // 23
  private:
    int move;
    explicit Move(int value) : move(value) {
    }
    enum { 
      Resign_VALUE = (1<<8), Declare_WIN = (2<<8),
      Black_PASS = 0, White_PASS = (-1)<<28, 
    };
  public:
    int intValue() const { return move; }
    /** 一局面辺りの合法手の最大値 
     * 重複して手を生成することがある場合は，600では不足かもしれない
     */
    static const unsigned int MaxUniqMoves=600;
  private:
    void init(Square from, Square to, Ptype ptype,
	      Ptype capture_ptype, bool is_promote, Player player)
    {
      move =  (to.uintValue()
 	       + (from.uintValue()<<8)
	       + (static_cast<unsigned int>(capture_ptype)<<16)
	       + (static_cast<unsigned int>(is_promote)<<BitOffsetPromote)
	       + (static_cast<unsigned int>(ptype)<<24)
	       + (Int(player)<<28));
    }
  public:
    Move() : move(Resign_VALUE)
    {
    }
    /** Resign でも PASS でもない. isValid()かどうかは分からない．*/
    bool isNormal() const { 
      // PASS や Resign は to() が 00
      return move & 0x00ff; 
    }
    bool isPass() const { return (move & 0xffff) == 0; }
    static const Move makeDirect(int value) { return Move(value); }
    static const Move PASS(Player P) { return Move(Int(P)<<28); }
    static const Move Resign() { return Move(Resign_VALUE); }
    static const Move DeclareWin() { return Move(Declare_WIN); }
    /**
     * 移動
     */
    Move(Square from, Square to, Ptype ptype,
	 Ptype capture_ptype, bool is_promote, Player player)
    {
      init(from, to, ptype, capture_ptype, is_promote, player);
    }
    /**
     * drop
     */
    Move(Square to, Ptype ptype, Player player)
    {
      init(Square::STAND(), to, ptype, Ptype_EMPTY, false, player);
    }
    const Square from() const 
    {
      assert(! isInvalid());
      const Square result = Square::makeDirect((move>>8) & 0xff);
      return result;
    }
    const Square to() const {
      assert(! isInvalid());
      const Square result = Square::makeDirect(move & 0xff);
      return result;
    }
    /**
     * pieceに使うためのmaskなので
     */
    int promoteMask() const {
      assert(isNormal());
      return (move & (1<<BitOffsetPromote));
    }
    bool isPromotion() const { assert(isNormal()); return (move & (1<<BitOffsetPromote))!=0; }
    bool isCapture() const { assert(isNormal()); return capturePtype() != Ptype_EMPTY; }
    bool isDrop() const { assert(isNormal()); return from().isPieceStand(); }
      
    Ptype ptype() const {
      assert(! isInvalid());
      const Ptype result = Ptype{(move >> 24) & 0xf};
      return result;
    }
    /** 移動後のPtype, i.e., 成る手だった場合成った後 */
    PtypeO ptypeO() const {
      assert(! isInvalid());
      const PtypeO result = PtypeO(move >> 24);
      return result;
    }
    /** 移動前のPtypeO, i.e., 成る手だった場合成る前 */
    PtypeO oldPtypeO() const {
      assert(! isInvalid());
      const PtypeO result = PtypeO((move>>24)+((move >> (BitOffsetPromote-3))&8));
      return result;
    }
    /** 移動前のPtype, i.e., 成る手だった場合成る前 */
    Ptype oldPtype() const { 
      assert(! isInvalid());
      const PtypeO old_ptypeo = PtypeO((move>>24)+((move >> (BitOffsetPromote-3))&8));
      return osl::ptype(old_ptypeo); 
    }
    Ptype capturePtype() const {
      assert(isNormal());
      return Ptype{(move>>16)&0xf};
    }
    PtypeO capturePtypeO() const {
      assert(isCapture());
      return newPtypeO(alt(player()), capturePtype());
    }
    PtypeO capturePtypeOSafe() const {
      if (! isCapture())
	return Ptypeo_EMPTY;
      return capturePtypeO();
    }

    Player player() const {
      assert(! isInvalid());
      return Player{move>>28};
    }
    bool isValid() const;
    /** state に apply 可能でない場合にtrue */
    bool isInvalid() const { 
      return static_cast<unsigned int>(move-1) < Declare_WIN; 
    }
    bool isValidOrPass() const { return isPass() || isValid(); }
    /** isNormal() and no violation with the shogi rules */
    bool is_ordinary_valid() const;
    /**
     * no capture moveからcapture moveを作る
     */
    const Move newAddCapture(Piece capture) const
    {
      assert(! isCapture());
      return makeDirect(intValue()+(capture.intValue()&0xf0000));
    }
    /**
     * promote moveからunpromote moveを作る
     */
    const Move unpromote() const {
      assert(isNormal());
      return makeDirect(intValue()^((1<<BitOffsetPromote)^(1<<27)));
    }
    /**
     * unpromote moveからpromote moveを作る
     */
    const Move promote() const {
      assert(isNormal());
      return makeDirect(intValue()^((1<<BitOffsetPromote)^(1<<27)));
    }
    static bool ignoreUnpromote(Player P, Ptype ptype,Square from,Square to) {
      switch(ptype) {
      case PAWN: 
	return to.isPromoteArea(P);
      case BISHOP: case ROOK: 
	return to.isPromoteArea(P) || from.isPromoteArea(P);
      case LANCE:
	return (P==BLACK ? to.y()==2 : to.y()==8);
      default:
        return false;
      }
    }
    /**
     * 合法手ではあるが，打歩詰め絡み以外では有利にはならない手.  
     */
    bool ignoreUnpromote(Player P) const {
      assert(player()==P);
      if(isDrop()) return false;
      return ignoreUnpromote(P, ptype(),from(),to());
    }
    bool ignoreUnpromote() const {
      return ignoreUnpromote(player());
    }
    /**
     * MoveをunpromoteするとcutUnpromoteなMoveになる
     */
    template<Player P>
    bool hasIgnoredUnpromote() const {
      assert(player()==P);
      if(!isPromotion()) return false;
      switch(ptype()) {
      case PPAWN: 
	return (P==BLACK ? to().y()!=1 : to().y()!=9);
      case PLANCE:
	return (P==BLACK ? to().y()==2 : to().y()==8);
      case PBISHOP: case PROOK: 
	return true;
      default: return false;
      }
    }
    bool hasIgnoredUnpromote() const {
      if(player()==BLACK) return hasIgnoredUnpromote<BLACK>();
      else return hasIgnoredUnpromote<WHITE>();
    }
    Move rotate180() const;
    friend inline bool operator==(Move, Move) = default;
    friend inline bool operator!=(Move, Move) = default;
  };
  std::ostream& operator<<(std::ostream& os, Move move);
}

namespace osl
{
  enum GameResult { BlackWin, WhiteWin, Draw, InGame };
  constexpr GameResult win_result(Player P) { return P == BLACK ? BlackWin : WhiteWin; }
  constexpr GameResult loss_result(Player P) { return P == BLACK ? WhiteWin : BlackWin; }
  constexpr bool has_winner(GameResult r) { return r == BlackWin || r == WhiteWin; }
  constexpr GameResult flip(GameResult r) {
    if (! has_winner(r)) return r;
    return (r == BlackWin) ? WhiteWin : BlackWin;
  }
}
#endif
// MINIOSL_BASIC_TYPE_H
