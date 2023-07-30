#ifndef MINIOSL_DETAILS_H
#define MINIOSL_DETAILS_H

#include "basic-type.h"
// contents that depend only on those in basic-type, i.e., not on states

namespace osl 
{
  constexpr int Offset32_Width=8;
  /**
   * 差が uniqになるような座標の差分.
   * x*32+y同士の差を取る
   * ちょっとだけ溢れても良い
   */
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
    extern const CArray<Offset, Offset32_SIZE> Base8_Offsets_Extended;
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
   *
   * 盤外の場合も Offset_ZERO になる KingVisibility など要注意
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

namespace osl 
{
  /**
   * bitset of file (x)
   */
  enum BitXmask : int { XNone=0 };
  inline void set_x(BitXmask& mask, Square sq) { mask = BitXmask(mask | one_hot(sq.x())); }
  inline void clear_x(BitXmask& mask, Square sq) { mask = BitXmask(mask & ~one_hot(sq.x())); }
  inline bool test_x(BitXmask& mask, Square sq) { return bittest((int)mask, sq.x()); }
  
  std::ostream& operator<<(std::ostream&,const BitXmask);
} // namespace osl

namespace osl 
{
  /** thread id */
  enum class TID { TID_ZERO=0 };
  using enum TID;
  constexpr int Int(TID tid) { return static_cast<int>(tid); }
  constexpr int idx(TID tid) { return Int(tid); }
}

#endif
// MINIOSL_DETAILS_H
