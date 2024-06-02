#ifndef MINIOSL_STATE_H
#define MINIOSL_STATE_H

#include "base-state.h"
#include "impl/effect.h"

// numEffectState.h
namespace osl
{
  namespace checkmate {
    enum King8Info : uint64_t;
  }
  using checkmate::King8Info;
  
  // need to replace when we enable game-tree search again.
  typedef std::vector<Move> MoveVector;
  std::ostream& operator<<(std::ostream&, const MoveVector&);
  /** rotate180 each element in place */
  void rotate180(MoveVector&);
  
  typedef std::vector<Piece> PieceVector;
  class EffectState;
  /**
   * equality independent of piece ids
   */
  bool operator==(const EffectState& st1, const EffectState& st2);

  /**
   * Standard state exported as `minioslcc.State`
   *
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
  public:
    // ----------------------------------------------------------------------
    // 0. 将棋以外の操作
    // ----------------------------------------------------------------------
    explicit EffectState(const BaseState& st=BaseState(HIRATE));
    ~EffectState();
    /** @internal 主要部分を高速にコピーする. 盤の外はコピーされない*/
    void copyFrom(const EffectState& src);
    bool check_internal_consistency() const;

    // ----------------------------------------------------------------------
    // 1. 盤面全体の情報
    // ----------------------------------------------------------------------
    /** return a set of piece IDs on board */
    PieceMask piecesOnBoard(Player p) const { return pieces_onboard[p]; }
    /** return a set of piece IDs promoted */
    PieceMask promotedPieces() const { return promoted; }
    /** return a set of piece IDs pinned */
    PieceMask pin(Player king) const {
      return pin_or_open[king]&piecesOnBoard(king);
    }
    /** @internal attack の駒で動くと開き王手になる可能性がある集合 */
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
    /** another loss condition for turn */
    bool inNoLegalMoves() const;
    /**
     * @internal
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
    /** @internal
     * 前の指手でeffectedPieces(pl)が変化したか.
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
    Piece findThreatenedPiece(Player P) const;

    template<Player P>
    BoardMask kingArea3x3() { return BoardMaskTable3x3[kingSquare<P>()]; };
    // ----------------------------------------------------------------------
    // 2. 駒に関する情報
    // ----------------------------------------------------------------------
    /** @internal return the furthest square of piece id num for direction d. */
    Square pieceReach(Direction d, int num) const {
      return effects.long_piece_reach.get(d,num);
    }
    /** return the furthest square of piece p for direction d.
     * i.e., assert(is_base8(d) && is_long_piece_id(piece.id()));
     * @param piece must have long move
     * @param dir must be consistent with p's move
     */
    Square pieceReach(Direction black_dir, Piece piece) const  {
      return pieceReach(black_dir, piece.id());
    }
    Square kingVisibilityBlackView(Player p, Direction d) const {
      return Square::makeDirect(king_visibility[p][d]);
    }
    /** 
     * return the furthest square visible p's King for direction d
     * @param p 注目する玉のプレイヤ
     * @param d piece からみた向き
     */
    Square kingVisibilityOfPlayer(Player p, Direction d) const {
      if (p == BLACK)
        d = inverse(d);
      return kingVisibilityBlackView(p, d);
    }
    /**
     * @internal
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
     * @internal
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
    /** return a set of piece IDs of color `P` cover the square `sq` */
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
    mask_t covering_pieces(Player P, Square target, Ptype ptype) const {
      return effectAt(target).selectBit(ptype) & piecesOnBoard(P).to_ullong();
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
     * @internal 対象とするマスにあるプレイヤーの利きがあるかどうか.
     * @tparam player 攻撃側
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
     * @internal 対象とするマスにあるプレイヤーの(ただしある駒以外)利きがあるかどうか.
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
     * @internal pinされている駒以外からの利きがある.
     */
    bool hasEffectByNotPinned(Player pl,Square target) const {
      assert(target.isOnBoard());
      PieceMask m=piecesOnBoard(pl)& ~pinOrOpen(pl) & effectAt(target);
      return m.any();
    }

    /**
     * @internal
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
     * @internal
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
     * @tparam P(template) 玉
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
     * @internal
     * @tparam P - 利きをつけている側のプレイヤ
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
     * @internal 利きの中から安そうな駒を選ぶ
     */
    Piece selectCheapPiece(PieceMask effect) const;
    /**
     * @internal
     * @param P - 利きをつけている側のプレイヤ
     * @param square - 調査する場所
     * @return 利きを付けている中で安そうな駒 (複数の場合でもEMPTYにはしない)
     */
    Piece findCheapAttack(Player P, Square square) const {
      return selectCheapPiece(piecesOnBoard(P) & effectAt(square));
    }
    /**
     * @internal 
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
     * test legal move
     * - valide win declaration, or
     * - move.is_ordinary_valid() + isAcceptable() + isSafeMove() + ! isPawnDropCheckmate()
     *
     * not implemented
     * - evasion in check
     * - repetition of states
     */
    bool isLegal(Move move) const;
    /** a part of legal move conditions  */
    bool isSafeMove(Move move) const;
    /** classify move property */
    bool isCheck(Move move) const;
    /** a part of illegal move conditions  */
    bool isPawnDropCheckmate(Move move) const;
    /** classify move property */
    bool isDirectCheck(Move move) const;
    /** classify move property */
    bool isOpenCheck(Move move) const;

    /**
     * moves accepted by makeMove(), similar but a bit different from isLegal()
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
    /** 自玉の詰めろを見つけられれば生成 */
    Move findThreatmate1ply() const;

    /** make a move to update the state */
    void makeMove(Move move);
    void makeMovePass() {
      changeTurn();
    }

    // sugars for reduce typing
    /** interpret string representation of move in usi or csa */
    Move to_move(std::string) const;
    /** make a move given in string, to update the state
     *
     * @code
     * osl::EffectState state;
     * state.make_move("+7776FU");
     * @endcode
     */
    void make_move(std::string csa_or_usi);
    
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
     * @internal
     * sq への利きを持つ各駒に関して処理を行う.
     */
    template<Player P,class Action>
    void forEachEffect(Square sq,Action & action) const {
      const PieceMask pieceMask=piecesOnBoard(P)&effectAt(sq);
      forEachEffect<Action>(pieceMask, sq, action);
    }
    /** 
     * @internal
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
     * @internal
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
     * @internal
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
     * @internal
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
