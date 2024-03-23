#ifndef MINIOSL_BASE_STATE_H
#define MINIOSL_BASE_STATE_H

#include "basic-type.h"
#include "impl/details.h"
#include <optional>


namespace osl
{
  enum Handicap{
    HIRATE,
    Shogi816K,
    //    KYOUOCHI,
    //    KAKUOCHI,
  };
  constexpr int Shogi816K_Size = 72 * 22680;
  class BaseState;
  std::ostream& operator<<(std::ostream& os,const BaseState& state);
  /**
   * equality allow permutation in piece ids
   */
  bool operator==(const BaseState& st1,const BaseState& st2);

  /** state without piece covers */
  class alignas(32) BaseState
  {
  protected:
    CArray<Piece,Square::SIZE> board;
    CArray<Piece,Piece::SIZE> pieces;
    CArray<PieceMask,2> stand_mask;
    CArray<BitXmask,2> pawnMask;
    CArray<CArray<char,basic_idx(Ptype(Ptype_SIZE))>,2> stand_count;

    /** 手番 */
    Player side_to_move;
    PieceMask used_mask;
  public:
    explicit BaseState();
    explicit BaseState(Handicap h, int additional_param=-1);
    // public継承させるので，virtual destructorを定義する．
    virtual ~BaseState();

    std::optional<int> shogi816kID() const;
    Piece pieceOf(int num) const { return pieces[num]; }
    inline auto all_pieces() const {
      return std::views::iota(0, Piece::SIZE)
        | std::views::transform([this](int n) { return this->pieceOf(n); });
    }
    inline auto long_pieces() const {
      return std::views::iota(ptype_piece_id[idx(LANCE)].first, Piece::SIZE)
        | std::views::transform([this](int n) { return this->pieceOf(n); });
    }
    /**
     * @param sq は isOnboardを満たす Square の12 近傍(8近傍+桂馬の利き)
     * ! sq.isOnBoard() の場合は `Piece_EDGE` を返す
     */
    Piece pieceAt(Square sq) const { return board[sq]; }
    Piece operator[](Square sq) const { return pieceAt(sq); }
    Piece pieceOnBoard(Square sq) const {
      assert(sq.isOnBoard());
      return pieceAt(sq);
    }
    bool isOnBoard(int id) const { return pieceOf(id).isOnBoard(); }

    template<Player P>
    Piece kingPiece() const { return pieceOf(king_piece_id(P)); }
    /** return piece of P's King */
    Piece kingPiece(Player P) const { return pieceOf(king_piece_id(P)); }
    template<Player P>
    Square kingSquare() const { return kingPiece<P>().square(); }
    Square kingSquare(Player P) const { return kingPiece(P).square(); }

    /** return a set of piece IDs on stand */
    const PieceMask& standMask(Player p) const { return stand_mask[p]; }
    const PieceMask& usedMask() const {return used_mask;}

    /** return whether player's pawn is on file x */
    bool pawnInFile(Player player, int x) const { return bittest(pawnMask[player], x); }
    template<Player P>
    bool pawnInFile(int x) const { return pawnInFile(P,x); }

    /** side to move */
    Player turn() const { return side_to_move; }
    /**
     * 手番を変更する
     */
    void changeTurn() { side_to_move = alt(side_to_move); }
    void setTurn(Player player) { side_to_move=player; }

    /** @internal */
    const Piece* getPiecePtr(Square sq) const { return &board[sq]; }
    /** lightweight validation of ordinary moves, primary intended for game record parsing.
     * @see EffectState::isAcceptable for piece reachability. */
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
     * @internal
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
    /** @internal set predefined initial state */
    void init(Handicap h, int additional_param=-1);
    void init816K(int id);
    /** @internal
     * make empty board for incremental initialization, typically consists of
     *  - setPiece() x40, and then
     *  - initFinalize()
     */
    void initEmpty();
    void initFinalize();

    void setPiece(Player player,Square sq,Ptype ptype);
    void setPieceAll(Player player);

    /** make a new rotated state */
    BaseState rotate180() const;

    bool check_internal_consistency() const;

    /** @internal
     * make a move assuming `this` is not an object of a derived class but a pure BaseState object. 
     * @warning result in undefined behavior if called in derived class or move is not acceptable.
     */
    void make_move_unsafe(Move);
  protected:
    void setBoard(Square sq,Piece piece) { board[sq]=piece; }
    void clearPawn(Player pl,Square sq) { clear_x(pawnMask[pl], sq); }
    void setPawn(Player pl,Square sq) { set_x(pawnMask[pl], sq); }
  private:
    int countPiecesOnStandBit(Player pl,Ptype ptype) const {
      return (standMask(pl) & PieceMask(piece_id_set(ptype))).countBit();
    }
    friend std::ostream& operator<<(std::ostream& os,const BaseState& state);
    friend bool operator==(const BaseState& st1,const BaseState& st2);
  };  

} // namespace osl


#endif
// MINIOSL_BASE_STATE_H
