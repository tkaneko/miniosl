#ifndef MINIOSL_STATE_H
#define MINIOSL_STATE_H

#include <array>
#include <cstdint>
#include <type_traits>
#include <vector>
#include <iosfwd>
#include <cassert>
#include <bit>
#include <ranges>
#include <numeric>

//#include "osl/basic_type.h"
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

  constexpr PtypeO Ptypeo_EMPTY=newPtypeO(BLACK,Ptype_EMPTY);
  constexpr PtypeO Ptypeo_EDGE __attribute__((unused)) = newPtypeO(WHITE,Ptype_EDGE);
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
    const Square rotate180() const {
      if (isPieceStand())
	return *this;
      Square ret=makeDirect(Square(9,9).uintValue()+Square(1,1).uintValue()-uintValue());
      return ret;
    }
  public:
    bool isPromoteArea(Player player) const { // promoteArea
      if (player==BLACK) 
	return (uintValue()&0xf)<=4;
      else 
	return (uintValue()&0x8)!=0;
    }
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
   * 圧縮していない moveの表現 .
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
   * - ptype    : 4 bit - promote moveの場合はpromote後のもの
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
    bool isPawnDrop() const {
      return isDrop() && ptype() == PAWN;
    }
      
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
    /** おおむね合法手 */
    bool isConsistent() const;
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
    const Move rotate180() const;
    friend inline bool operator==(Move, Move) = default;
    friend inline bool operator!=(Move, Move) = default;
  };
  std::ostream& operator<<(std::ostream& os, Move move);
}

// #include "osl/bits/ptypeTable.h"
namespace osl 
{
  /**
   * 差が uniqになるような座標の差分.
   * x*32+y同士の差を取る
   * ちょっとだけ溢れても良い
   */
  constexpr int Offset32_Width=8;
  enum Offset32 {
    Offset32_Min = -(Offset32_Width*32 + Offset32_Width),
    Offset32_Max = (Offset32_Width*32 + Offset32_Width),
  };
  constexpr unsigned int Offset32_SIZE=(Offset32_Max-Offset32_Min+1);
  inline auto to_offset32(Square to, Square from)
  { return Offset32(to.indexForOffset32()-from.indexForOffset32()); }
  constexpr inline auto to_offset32(int dx,int dy) { return Offset32(dx*32+dy); }
  constexpr inline auto idx(Offset32 o) { return o - Offset32_Min; }
  constexpr inline bool is_valid(Offset32 o) { return Offset32_Min <= o && o <= Offset32_Max; }
  constexpr inline auto Int(Offset32 o) { return static_cast<int>(o); }
  constexpr inline Offset32 operator-(Offset32 o) { return Offset32(-Int(o)); }
  /**
   * Player P からみた offset を黒番のものに変更する
   */
  template<Player P>
  constexpr inline Offset32 change_view(Offset32 o) { return P == BLACK ? o : Offset32(-o); }
} // namespace osl
namespace osl
{
  // property for each [PtypeO, Offset32]
  enum EffectDirection : int {
    EffectNone=0,
    EffectDefinite=1,
    // others: onestep-offset-for-long-move <<1
  };
  constexpr inline EffectDirection pack_long_neighbor(Offset offset) { 
    return EffectDirection((Int(offset) << 1)+1); 
  }
  constexpr inline EffectDirection pack_long_far(Offset offset) { 
    return EffectDirection(Int(offset) << 1); 
  }
  /**
   * 短い利きがある．長い利きの隣も含む
   */
  constexpr inline bool is_definite(EffectDirection effect) { return (effect & 1); }
  /**
   * 返り値が0なら長い利きがない, 
   * 0以外なら辿るのに必要なoffset
   */
  constexpr inline Offset to_offset(EffectDirection effect) { return Offset(effect >> 1); }
  constexpr inline bool has_long(EffectDirection effect) { return (effect & (-effect) & ~1); }
} // namespace osl
// #include "osl/bits/ptypeTraits.h"
namespace osl
{  
  constexpr std::array<int, Ptype_SIZE> ptype_promote_start_y = {{
      0, 0,
      0, 0, 0, 0, 0, 0,
      0, 0, 4, 9, 5, 4, 9, 9,
    }};

  /**
    for (auto n: BitRange(1024+128+16+4)) std::cout << n << '\n';
    => 2,4,7,10
   */
  struct BitIterator {
    using difference_type = std::ptrdiff_t;
    using value_type = int;
    BitIterator(mask_t init=0) : bs(init) {}
    bool operator == (const BitIterator&) const = default;
    value_type operator *() const { return std::countr_zero(bs); }
    BitIterator& operator++() {
      bs &= bs-1;
      return *this;
    }
    BitIterator operator++(int) {
      auto copy = *this;
      operator++();
      return copy;
    }
    mask_t bs;
  };
  struct BitRange {
    BitRange(uint64_t init) : bs(init) {}
    BitIterator begin() const { return BitIterator(bs); }
    BitIterator end() const { return BitIterator(0); }
    mask_t bs;
  };
  static_assert(std::ranges::input_range<BitRange>);
  inline auto to_range(mask_t m) { return BitRange(m); }

  constexpr mask_t piece_id_set(Ptype T) {
    auto [l, r] = ptype_piece_id[idx(T)];
    return (-1LL<<(l)) ^ (-1LL<<(r));
  }  

  constexpr bool has_move(Ptype T,Direction D) {
    auto dirset = ptype_move_direction[idx(T)];
    return bittest(dirset, D) || bittest(dirset, to_long(D));
  }

  inline bool legal_drop_at(Player P, Ptype T, Square sq) {
    auto yrange = ptype_drop_range[idx(T)];
    if (yrange.first == 1)
      return true; 

    if (P==BLACK)
      return sq.y() >= yrange.first;
    else
      return sq.y() <= ptype_drop_range_white[idx(T)].first;
  }
  
} // namespace osl
namespace osl
{
  template <class T, int N>
  class alignas(16) CArray : public std::array<T, N>
  {
    typedef std::array<T, N> parent_t;
  public:
    const T& operator[](Player pl) const { return parent_t::operator[](idx(pl)); }
    T& operator[](Player pl) { return parent_t::operator[](idx(pl)); }
    const T& operator[](Direction d) const { return parent_t::operator[](idx(d)); }
    T& operator[](Direction d) { return parent_t::operator[](idx(d)); }
    const T& operator[](Square sq) const { return parent_t::operator[](sq.index()); }
    T& operator[](Square sq) { return parent_t::operator[](sq.index()); }
    const T& operator[](size_t n) const { return parent_t::operator[](n); }
    T& operator[](size_t n) { return parent_t::operator[](n); }
  };

  template <class T, int A, int B>
  class CArray2d : public CArray<CArray<T, B>, A>
  {
  public:
    void fill(const T& value=T()) {
      for (auto& e: *this)
        e.fill(value);
    }
  };

  namespace board {
    // Offset32 => data
    // BoardTable 
    extern const CArray<Direction,Offset32_SIZE> Long_Directions;
    extern const CArray<Offset, Offset32_SIZE> Basic10_Offsets;
    extern const CArray<Offset, Offset32_SIZE> Base8_Offsets_Rich;
    extern const CArray<signed char, OnBoard_Offset_SIZE> Base8_Offsets;
    extern const CArray<unsigned char, OnBoard_Offset_SIZE> Base8_Directions;
    // PtypeTable
    extern const CArray2d<EffectDirection,Ptypeo_SIZE,Offset32_SIZE> Ptype_Effect_Table;
  }
  
  /**
   * Direction に変換.
   * @return offset32に対応する Direction (is_long)、対応がなければ UL (!is_long) 
   * @param P どちらのPlayerにとっての方向かを指定
   */
  template <Player P>
  inline Direction to_long_direction(Offset32 offset32) {
    assert(is_valid(offset32));
    return board::Long_Directions[idx(change_view<P>(offset32))];
  }
  /** @param P どちらのPlayerにとっての方向かを指定 */
  template <Player P>
  inline Direction to_long_direction(Square from, Square to) {
    return to_long_direction<P>(to_offset32(to,from));
  }
  inline Direction to_long_direction(Player P, Offset32 offset32) {
    if (P == BLACK) 
      return to_long_direction<BLACK>(offset32);
    else
      return to_long_direction<WHITE>(offset32);
  }

  /**
   * Longの利きの可能性のあるoffsetの場合は, 反復に使う offsetを
   * Shortの利きのoffsetの場合はそれ自身を返す.
   * 利きの可能性のない場合は0を返す
   */
  inline Offset basic_step(Offset32 offset32) {
    assert(is_valid(offset32));
    return board::Basic10_Offsets[idx(offset32)];
  }
  /**
   * Longの利きの可能性のあるoffsetの場合は, 反復に使う offsetを
   * Knight以外のShortの利きのoffsetの場合はそれ自身を返す.
   * Knightの利き, 利きの可能性のない場合は0を返す
   */
  inline Offset base8_step(Offset32 offset32) { return board::Base8_Offsets_Rich[idx(offset32)]; }
  inline Offset base8_step(Square to, Square from) { return base8_step(to_offset32(to, from)); }
  
  inline int onboard_offset_index(Square l, Square r) {
    return (int)(l.uintValue())-(int)(r.uintValue())-OnBoard_Offset_MIN;
  }
  /**
   * 8方向にいない場合の値は実装依存．
   */
  template<Player P>
  inline Direction base8_dir_unsafe(Square from, Square to) {
    if constexpr (P==BLACK)
      return Direction(board::Base8_Directions[onboard_offset_index(to, from)]);
    else
      return Direction(board::Base8_Directions[onboard_offset_index(from, to)]);
  }
  inline Direction base8_dir_unsafe(Player P, Square from, Square to) {
    if (P==BLACK)
      return base8_dir_unsafe<BLACK>(from, to);
    else
      return base8_dir_unsafe<WHITE>(from, to);
  }
  template<Player P>
  inline Direction base8_dir(Square from, Square to) {
    assert(from.isOnBoard() && to.isOnBoard());
    assert(from.x()==to.x() || from.y()==to.y() || abs(from.x()-to.x())==abs(from.y()-to.y()));
    return base8_dir_unsafe<P>(from,to);
  }

  template<Player P>
  inline auto base8_dir_step(Square from, Square to) {
    assert(from.isOnBoard() && to.isOnBoard());
    assert(from.x()==to.x() || from.y()==to.y() || 
           abs(from.x()-to.x())==abs(from.y()-to.y()));
    int idx=onboard_offset_index(to, from);
    Offset o=Offset(board::Base8_Offsets[idx]);
    Direction d{board::Base8_Directions[idx]};
    if constexpr (P==BLACK)
      return std::make_pair(d, o);
    else
      return std::make_pair(inverse(d), o);
  }
  inline bool is_between_unsafe(Square t,Square p0,Square p1) {
    int i1 = onboard_offset_index(t, p0), i2 = onboard_offset_index(p1, t);
    assert(board::Base8_Directions[i1]!=Direction_INVALID_VALUE
           || board::Base8_Directions[i2]!=Direction_INVALID_VALUE);
    return board::Base8_Directions[i1]==board::Base8_Directions[i2];
  }
  inline bool is_between_safe(Square t,Square p0,Square p1) {
    return (base8_step(t, p0) == Offset_ZERO) ? false : is_between_unsafe(t, p0, p1);
  }

  inline auto ptype_effect(PtypeO ptypeo,Offset32 offset32) {
    return board::Ptype_Effect_Table[idx(ptypeo)][idx(offset32)];
  }
  /** 
   * fromにいるptypeoがtoに利きを持つか?
   * @param ptypeo - 駒の種類
   * @param from - 駒の位置
   * @param to - 利きをチェックするマスの位置
   */
  inline EffectDirection ptype_effect(PtypeO ptypeo,Square from, Square to) {
    assert(from.isOnBoard() && to.isOnBoard());
    return ptype_effect(ptypeo, to_offset32(to,from));
  }
  inline bool has_definite_effect(PtypeO attacker, Square from, Square to) {
    return is_definite(ptype_effect(attacker, from, to));
  }
} // namespace osl

// #include "osl/bits/boardTable.h"
namespace osl
{
  /**
   * 駒番号のビットセットを64bit整数で表現.
   * 各メソッドの引数 num は駒番号.
   * ここでは [0,39] を使うが、EffectPieceMask で拡張
   */
  class PieceMask 
  {
  private:
    uint64_t mask;
  public:
    explicit PieceMask(uint64_t value=0uLL) : mask(value) {
    }
    void resetAll() { mask = 0uLL; }
    void setAll() { mask = 0xffffffffffuLL; }
    PieceMask& operator^=(const PieceMask& o) {
      mask ^= o.mask;
      return *this;
    }
    PieceMask& operator&=(const PieceMask& o) {
      mask &= o.mask;
      return *this;
    }
    PieceMask& operator|=(const PieceMask& o) {
      mask |= o.mask;
      return *this;
    }
    PieceMask& operator-=(const PieceMask& o) {
      mask -= o.mask;
      return *this;
    }
    PieceMask& operator+=(const PieceMask& o) {
      mask += o.mask;
      return *this;
    }
    uint64_t to_ullong() const { return mask; }
    bool none() const { return mask == 0; }
    bool hasMultipleBit() const { return has_multiple_bit(mask); }
    /**
     * bit の数を2まで数える
     * @return 0,1,2 (2の場合は2以上)
     */
    int countBit2() const 
    {
      if (none()) 
	return 0;
      // todo 
      return countBit();
    }
    int countBit() const { return std::popcount(mask); }
    int takeOneBit() {
      assert(!none());
      return osl::take_one_bit(mask);
    }
    auto toRange() const { return to_range(to_ullong()); }
    
    bool test(int num) const { return mask & one_hot(num); }
    void set(int num) { mask |= one_hot(num); }
    void flip(int num) { mask ^= one_hot(num); }
    void reset(int num) { mask &= ~one_hot(num); }
    bool any() const { return ! none(); }

    /** unpromote(PTYPE) の駒のbit だけ取り出す */
    template <Ptype PTYPE> mask_t selectBit() const { return mask & piece_id_set(PTYPE); }
    /** unpromote(PTYPE) の駒のbit を消す */
    template <Ptype PTYPE> void clearBit() { mask &= ~piece_id_set(PTYPE); }
    /** unpromote(PTYPE) の駒のbit を立てる */
    template <Ptype PTYPE> void setBit() { mask |= piece_id_set(PTYPE); }
    friend bool inline operator==(PieceMask, PieceMask) = default;
    friend bool inline operator!=(PieceMask, PieceMask) = default;
    friend const PieceMask operator&(const PieceMask &m1, const PieceMask &m2);
    friend const PieceMask operator|(const PieceMask &m1, const PieceMask &m2);
    friend const PieceMask operator~(const PieceMask &m1);
  };

  inline const PieceMask operator&(const PieceMask &m1, const PieceMask &m2) {
    return PieceMask(m1.mask & m2.mask);
  }
  inline const PieceMask operator|(const PieceMask &m1, const PieceMask &m2) {
    return PieceMask(m1.mask | m2.mask);
  }
  inline const PieceMask operator~(const PieceMask &m1) {
    return PieceMask(~m1.mask);
  }

  std::ostream& operator<<(std::ostream& os,PieceMask const& pieceMask);
} // namespace osl


// #include "osl/bits/bitXmask.h"
namespace osl 
{
  /**
   * X座標のbitset
   */
  enum BitXmask : int { XNone=0 };
  inline void set_x(BitXmask& mask, Square sq) { mask = BitXmask(mask | one_hot(sq.x())); }
  inline void clear_x(BitXmask& mask, Square sq) { mask = BitXmask(mask & ~one_hot(sq.x())); }
  inline bool test_x(BitXmask& mask, Square sq) { return bittest((int)mask, sq.x()); }
  
  std::ostream& operator<<(std::ostream&,const BitXmask);
} // namespace osl

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
   * 盤上の駒のみを比較する（持ち駒は見ない）.
   * なお、駒番に非依存な局面比較をしたい場合は、osl::record::CompactBoardや
   * osl::hash::HashKeyを用いる.
   */
  bool operator==(const BaseState& st1,const BaseState& st2);
  
  class alignas(32) BaseState
  {
  private:
    friend std::ostream& operator<<(std::ostream& os,const BaseState& state);
    friend bool operator==(const BaseState& st1,const BaseState& st2);
    typedef BaseState state_t;
  public:
    static const bool hasPawnMask=true;
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
    // 生成に関するもの
    explicit BaseState();
    explicit BaseState(Handicap h);
    // public継承させるので，virtual destructorを定義する．
    virtual ~BaseState();
    /** set predefined initial state */
    void init(Handicap h);
    /** make empty board ready for manual initialization via setPiece() */
    void initEmpty();
    void initFinalize();
  public:
    const Piece pieceOf(int num) const { return pieces[num]; }
    inline auto all_pieces() const {
      return std::views::iota(0, Piece::SIZE)
        | std::views::transform([this](int n) { return this->pieceOf(n); });
    }
    inline auto long_pieces() const {
      return std::views::iota(ptype_piece_id[idx(LANCE)].first, Piece::SIZE)
        | std::views::transform([this](int n) { return this->pieceOf(n); });
    }
    template<Player P>
    const Piece kingPiece() const { return pieceOf(king_piece_id(P)); }
    const Piece kingPiece(Player P) const { return pieceOf(king_piece_id(P)); }
    template<Player P>
    Square kingSquare() const { return kingPiece<P>().square(); }
    Square kingSquare(Player P) const { return kingPiece(P).square(); }
    void setBoard(Square sq,Piece piece) { board[sq]=piece; }
  public:
    const PieceMask& standMask(Player p) const { return stand_mask[p]; }
    const PieceMask& usedMask() const {return used_mask;}
    // protected:
    /** (internal) */
    void clearPawn(Player pl,Square sq) { clear_x(pawnMask[pl], sq); }
    /** (internal) */
    void setPawn(Player pl,Square sq) { set_x(pawnMask[pl], sq); }
  public:      
    bool pawnInFile(Player player, int x) const { return bittest(pawnMask[player], x); }

    template<Player P>
    bool pawnInFile(int x) const { return pawnInFile(P,x); }

    void setPiece(Player player,Square sq,Ptype ptype);
    void setPieceAll(Player player);

    /**
     * @param sq は isOnboardを満たす Square の12近傍(8近傍+桂馬の利き)
     * ! isOnBoard(sq) の場合は Piece_EDGE を返す
     */
    const Piece pieceAt(Square sq) const { return board[sq]; }
    const Piece operator[](Square sq) const { return pieceAt(sq); }
    const Piece* getPiecePtr(Square sq) const { return &board[sq]; }
    const Piece pieceOnBoard(Square sq) const {
      assert(sq.isOnBoard());
      return pieceAt(sq);
    }
    bool isOnBoard(int num) const { return pieceOf(num).isOnBoard(); }
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
  private:
    int countPiecesOnStandBit(Player pl,Ptype ptype) const {
      return (standMask(pl) & PieceMask(piece_id_set(ptype))).countBit();
    }
  public:    
    void setTurn(Player player) { player_to_move=player; }
    Player turn() const { return player_to_move; }
    /**
     * 手番を変更する
     */
    void changeTurn() { player_to_move = alt(player_to_move); }
    bool isConsistent() const;
  public:
    /**
     * @param from - マスの位置
     * @param to - マスの位置
     * @param offset - fromからtoへのshort offset
     * fromとtoがクイーンで利きがある位置関係にあるという前提
     * で，間が全部空白かをチェック
     * @param pieceExistsAtTo - toに必ず駒がある (toが空白でも動く)
     */
    bool isEmptyBetween(Square from, Square to,Offset offset,bool pieceExistsAtTo=false) const
    {
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
    const BaseState rotate180() const;
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
   * 駒番号に依存した局面（インスタンス）比較をする.
   * なお、駒番に非依存な局面比較をしたい場合は、osl::record::CompactBoardや
   * osl::hash::HashKeyを用いる.
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
    bool isConsistent() const;

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
    // --- 2014/03
    /**
     * 合法手かどうか = move.isConsistent + isLegalLight + isSafe + !isPawnDropCheckmate
     * 王手中の王手回避と千日手かどうかは検査しない．
     */
    bool isLegal(Move move) const;
    bool isSafeMove(Move move) const;
    bool isCheck(Move move) const;
    bool isPawnDropCheckmate(Move move) const;
    bool isDirectCheck(Move move) const;
    bool isOpenCheck(Move move) const;
    // ---

    /**
     * 合法手かどうかを簡単に検査する．局面に依存するチェックのみ．
     * ルール上指せない手である可能性がある場合は，isLegal を用いる．
     *
     * 局面に依存する検査でも，玉の素抜きや王手を防いでいるか，
     * 千日手，打歩詰かどうかは検査しない．
     * @param showError(template) - falseを返す時には何か表示する.
     * @param move - 手
     */
    bool isLegalLight(Move move) const;

    /**
     * 全ての合法手を生成する. 玉の素抜きや打歩詰の確認をする．
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
