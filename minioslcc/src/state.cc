#include "state.h"
#include "record.h"
#include "impl/checkmate.h"
#include "impl/more.h"
#include "impl/rng.h"
#include <iostream>
#include <algorithm>
#include <cstdlib>

std::ostream& osl::operator<<(std::ostream& os, const MoveVector& moves) {
  os << "MoveVector(" << moves.size() << ") ";
  for (auto m: moves)
    os << to_csa(m);
  return os << "\n";
}

void osl::rotate180(MoveVector& moves) {
  for (auto& e: moves)
    e = e.rotate180();
}

// numEffectState.cc
bool osl::operator==(const EffectState& st1, const EffectState& st2) {
  assert(st1.check_internal_consistency());
  assert(st2.check_internal_consistency());
  // if consistent, sufficient to see the equality of the base class
  return (static_cast<const BaseState&>(st1) == static_cast<const BaseState&>(st2));
}

template<osl::Player P>
void osl::EffectState::makeKing8Info()
{
  king8infos[P] = King8Info{0};
  if (kingSquare<P>().isPieceStand())
    return;
  king8infos[P]=to_king8info<alt(P)>(*this,kingSquare<P>());
}

osl::
EffectState::EffectState(const BaseState& st) 
  : BaseState(st),effects(st)
{
  pieces_onboard[0].resetAll();
  pieces_onboard[1].resetAll();
  promoted.resetAll();
  effects.e_pieces[0].resetAll();
  effects.e_pieces[1].resetAll();
  effects.e_pieces_modified[0].resetAll();
  effects.e_pieces_modified[1].resetAll();
  for (int num: all_piece_id()) {
    Piece p=pieceOf(num);
    if (p.isOnBoard()){
      pieces_onboard[p.owner()].set(num);
      if (p.isPromoted())
	promoted.set(num);
      for (auto pl: players){
	if(hasEffectAt(pl,p.square()))
          {
            effects.e_pieces[pl].set(num);
            effects.e_pieces_modified[pl].set(num);
          }
      }
    }
  }
  setPinOpen(BLACK);
  setPinOpen(WHITE);
  makeKing8Info<BLACK>();
  makeKing8Info<WHITE>();
}
osl::
EffectState::~EffectState() 
{
}

osl::Piece osl::
EffectState::selectCheapPiece(PieceMask effect) const
{
  if (! effect.any())
    return Piece::EMPTY();
  mask_t pieces = effect.selectBit<PAWN>(), ppieces;
  if (pieces) {
    ppieces = pieces & promoted.to_ullong();
    pieces &= ~ppieces;
    if (pieces)
      return pieceOf(std::countr_zero(pieces));
    return pieceOf(std::countr_zero(ppieces));
  }
  pieces = effect.selectBit<LANCE>();
  if (pieces) {
    ppieces = pieces & promoted.to_ullong();
    pieces &= ~ppieces;
    if (pieces)
      return pieceOf(std::countr_zero(pieces));
    return pieceOf(std::countr_zero(ppieces));
  }
  mask_t king = effect.selectBit<KING>();
  effect.clearBit<KING>();
  if (effect.none())
    return pieceOf(std::countr_zero(king));
  // depends on current piece numbers: <FU 0>, KE 18, GI 22, KI 26, <OU 30>, <KY 32>, KA 36, HI 38, 
  ppieces = (effect & promoted).to_ullong();
  pieces = effect.to_ullong() & ~ppieces;
  if (pieces == 0 || ppieces == 0)
    return pieceOf(std::countr_zero(pieces ? pieces : ppieces));
  const int num = std::countr_zero(pieces), nump = std::countr_zero(ppieces);
  if (piece_id_ptype[num] == piece_id_ptype[nump])
    return pieceOf(num);
  return pieceOf(std::min(num, nump));
}

inline int bsr64(uint64_t val) 
{
  return 63-std::countl_zero(val);
}

osl::Piece osl::
EffectState::findThreatenedPiece(Player P) const
{
  assert(! inCheck(P));
  PieceMask pieces = piecesOnBoard(P) & effectedPieces(alt(P));
  PieceMask nolance = pieces; nolance.clearBit<LANCE>();
  int pp=-1, npp=-1, ret=-1;
  {
    mask_t all = nolance.to_ullong();
    mask_t promoted = all & promotedPieces().to_ullong();
    mask_t notpromoted = all & ~promoted;
    if (promoted) {
      pp = bsr64(promoted);
      notpromoted &= ~ piece_id_set(piece_id_ptype[pp]);
    }
    if (notpromoted)
      npp = bsr64(notpromoted);
    ret = std::max(pp, npp);
    if (ret >= ptype_piece_id[Int(KNIGHT)].first)
      return pieceOf(ret);  
  }
  mask_t lance = pieces.selectBit<LANCE>();
  if (lance) {
    mask_t plance = lance & promotedPieces().to_ullong();
    if (plance)
      return pieceOf(bsr64(plance));
    return pieceOf(bsr64(lance));
  }
  if (ret >= 0) {
    assert(piece_id_ptype[ret] == PAWN);
    return pieceOf(ret);
  }
  return Piece::EMPTY();
}

osl::Move osl::EffectState::to_move(std::string csa_or_usi) const {
  try {
    return usi::to_move(csa_or_usi, *this);
  }
  catch (std::exception& e) {
  }
  try {
    return csa::to_move(csa_or_usi, *this);
  }
  catch (std::exception& e) {
  }
  throw std::domain_error("not acceptable "+csa_or_usi);
}

void osl::EffectState::make_move(std::string csa_or_usi) {
  auto move = to_move(csa_or_usi);
  if (move.isPass() || isAcceptable(move))
    makeMove(move);
  else
    throw std::domain_error("not acceptable "+csa_or_usi);
}


void osl::EffectState::makeMove(Move move) {
  assert(turn() == move.player());
  effects.clearPast();

  if (move.isPass()) {
    makeMovePass();
    return;
  }

  assert(isAcceptable(move));
  const Square from=move.from(), to=move.to();
  const auto pin_or_open_backup = pin_or_open;
  const auto player_to_move = this->turn();
  int num;                      /* to be initilized in both branches of drop and non-drop */
  if (from.isPieceStand()) {
    auto ptype = move.ptype();
    num = (player_to_move == BLACK) ? doDropMove<BLACK>(to,ptype) : doDropMove<WHITE>(to,ptype);
    recalcPinOpen(to, BLACK);
    recalcPinOpen(to, WHITE);
    if (ptype==PAWN)
      setPawn(turn(),to);
  }
  else { // onboard
    Piece old_piece = pieceAt(from);
    num=old_piece.id();
    const Piece captured = pieceOnBoard(to);
    int move_promote_mask = move.promoteMask();
    Piece new_piece=old_piece.move((to-from), move_promote_mask);
    pieces[num] = new_piece;
    if (captured != Piece::EMPTY()) {
      const int captured_id = captured.id();
      pieces[captured_id] = captured.captured();
      effects.setSourceChange(effectAt(to));
      if (player_to_move == BLACK)
        doCaptureMove<BLACK>(from,to,captured,move_promote_mask, old_piece, new_piece, num, captured_id);
      else
        doCaptureMove<WHITE>(from,to,captured,move_promote_mask, old_piece, new_piece, num, captured_id);
      promoted.reset(captured_id);
      effects.e_pieces[BLACK].reset(captured_id);
      effects.e_pieces[WHITE].reset(captured_id);
      if (captured.ptype() == PAWN)
        clearPawn(alt(turn()),to);
    }
    else {
      if (player_to_move == BLACK)
        doSimpleMove<BLACK>(from,to,move_promote_mask, old_piece, new_piece, num);
      else
        doSimpleMove<WHITE>(from,to,move_promote_mask, old_piece, new_piece, num);
    }
    // onboard moves reach here
    if (move_promote_mask) {
      promoted.set(num);
      if (num < ptype_piece_id[Int(PAWN)].second)
        clearPawn(turn(),from);
    }
  }
  // all moves reach
  if(hasEffectAt(BLACK,to))
    effects.e_pieces[BLACK].set(num);
  else
    effects.e_pieces[BLACK].reset(num);
  if (hasEffectAt(WHITE,to))
    effects.e_pieces[WHITE].set(num);
  else
    effects.e_pieces[WHITE].reset(num);
  effects.e_pieces_modified[BLACK].set(num);
  effects.e_pieces_modified[WHITE].set(num);

  BoardMask changed = changedEffects();
  changed.set(from);
  changed.set(to);
  if (changed.anyInRange(kingArea3x3<BLACK>()) || pin_or_open[BLACK]!=pin_or_open_backup[BLACK])
    makeKing8Info<BLACK>();
  if (changed.anyInRange(kingArea3x3<WHITE>()) || pin_or_open[WHITE]!=pin_or_open_backup[WHITE])
    makeKing8Info<WHITE>();

  changeTurn();
}

template<osl::Player P>
void osl::EffectState::
doSimpleMove(Square from, Square to, int promoteMask, Piece old_piece, Piece new_piece, int num)
{
  const PtypeO old_ptypeo=old_piece.ptypeO(), new_ptypeo=new_piece.ptypeO();
  
  // 自分自身の効きを外す
  effects.doEffect<P,EffectSub>(*this, old_ptypeo, from, num);
  // 自分自身がブロックしていたpromote?の延長
  // あるいは自分自身のブロック
  effects.pp_long_state.clear(num);
  setBoard(to, new_piece);
  effects.doBlockAt<EffectSub>(*this, to, num);
  setBoard(from, Piece::EMPTY());
  effects.doBlockAt<EffectAdd>(*this, from, num);
  effects.doEffect<P,EffectAdd>(*this, new_ptypeo, to, num);

  if (old_ptypeo == newPtypeO(P, KING))
    setPinOpen(P);
  else {
    pin_or_open[P].reset(num);
    updatePinOpen(from, to, P);
  }
  pin_or_open[alt(P)].reset(num);
  updatePinOpen(from, to, alt(P));
}

template<osl::Player P>
void osl::EffectState::
doCaptureMove(Square from, Square to, Piece target, int promoteMask, Piece old_piece, 
              Piece new_piece, int num0, int num1) {
  const mask_t num1Mask=one_hot(num1);
  pieces_onboard[alt(P)] ^= PieceMask(num1Mask);
  stand_mask[P] ^= PieceMask(num1Mask);

  const auto old_ptypeo=old_piece.ptypeO(), new_ptypeo=new_piece.ptypeO();
  const auto capturePtypeO=target.ptypeO();
  stand_count[P][basic_idx(unpromote(ptype(capturePtypeO)))]++;
  effects.doEffect<alt(P),EffectSub>(*this, capturePtypeO, to, num1);
  effects.doEffect<P,EffectSub>(*this, old_ptypeo, from, num0);
  setBoard(from,Piece::EMPTY());
  effects.doBlockAt<EffectAdd>(*this, from, num0);
  effects.pp_long_state[num0]=effects.pp_long_state[num1];
  effects.pp_long_state.clear(num1);
  setBoard(to,new_piece);
  effects.doEffect<P,EffectAdd>(*this, new_ptypeo, to, num0);

  if (old_ptypeo == newPtypeO(P, KING))
    setPinOpen(P);
  else {
    pin_or_open[P].reset(num0);
    pin_or_open[P].reset(num1); // captured is not pin
    updatePinOpen(from, to, P);
  }
  pin_or_open[alt(P)].reset(num0);
  pin_or_open[alt(P)].reset(num1); // captured is not pin
  updatePinOpen(from, to, alt(P));
}

template<osl::Player P>
int osl::EffectState::
doDropMove(Square to, Ptype ptype)
{
  const mask_t mochigoma= standMask(P).to_ullong() & piece_id_set(ptype);
  assert(mochigoma);
  int num = std::countr_zero(mochigoma);
  mask_t num_one_hot=lowest_bit(mochigoma);
  Piece new_piece=pieceOf(num).drop(to);
  assert(0 <= num && num < 40 && num_one_hot == one_hot(num) && new_piece.id() == num);
  const auto ptypeO=new_piece.ptypeO();
  pieces[num] = new_piece;
  effects.doBlockAt<EffectSub>(*this, to, num);
  effects.doEffect<P,EffectAdd>(*this, ptypeO, to, num);
  setBoard(to, new_piece);
  stand_mask[P] ^= PieceMask(num_one_hot);
  stand_count[P][basic_idx(ptype)]--;
  pieces_onboard[P] ^= PieceMask(num_one_hot);
  return num;
}

bool osl::EffectState::check_internal_consistency() const {
  if (!BaseState::check_internal_consistency()) 
    return false;
  EffectSummary effects1(*this);
  if (!(effects1==effects)) 
    return false;
  for (Player p: players) {
    if (kingSquare(p).isPieceStand())
      continue;
    King8Info king8info2 = to_king8info(alt(p), *this);
    if (king8Info(p) != king8info2) 
      return false;
  }
  for (Piece p: all_pieces()) {
    if (p.isOnBoard()) 
      if (promoted.test(p.id()) != p.isPromoted()) 
	return false;
  }
  for (auto p: long_pieces()) {
    if (! p.isOnBoard() || p.ptype() == PLANCE)
      continue;
    for (auto d: base8_directions()) { // black view
      auto dst = effects.long_piece_reach.get(d, p.id());
      if (dst.isPieceStand())
        continue;
      auto target = pieceAt(dst);
      auto src = p.square();
      auto step = to_offset(BLACK, d);
      if (! bittest(ptype_move_direction[idx(unpromote(p.ptype()))], change_view(p.owner(), to_long(d)))) // player_view
        continue;
      auto step2 = basic_step(to_offset32(dst-step, src));

      if (src+step != dst)
        if (step != step2 || ! isEmptyBetween(src, dst-step, step))
          return false;
      if (target.isPiece()) {
        if (!hasEffectByPiece(p, dst) || effects.pp_long_state[target.id()][d] != p.id())
          return false;
      }
    }
  }
  for (auto p: all_pieces()) {
    if (! p.isOnBoard())
      continue;
    for (auto d: base8_directions()) { // black view
      auto attack = effects.pp_long_state[p.id()][d];
      if (attack == Piece_ID_EMPTY)
        continue;
      auto attack_piece = pieceOf(attack);
      if (! attack_piece.isOnBoard() || ! hasEffectByPiece(attack_piece, p.square()))
        return false;
    }
  }
  for (auto pl: players) {
    auto king = kingSquare(pl);
    if (! king.isOnBoard())
      continue;
    for (auto d: base8_directions()) { // black view
      Square reach = kingVisibilityBlackView(pl, d);
      auto step = to_offset(BLACK, d); // view: reach -> king
      if (king == reach+step) continue;
      auto step2 = basic_step(to_offset32(king, reach+step));
      if (step != step2 || ! isEmptyBetween(reach+step, king, step))
        return false;      
    }
  }
  return true;
}

bool osl::EffectState::isAcceptable(Move move) const {
  if (move == move.PASS(turn()))
    return true;
  
  if (! move.is_ordinary_valid() || ! move_is_consistent(move))
    return false;
  if (! move.isDrop() && ! hasEffectByPiece(pieceAt(move.from()), move.to())) 
    return false;

  return true;
}
bool osl::EffectState::isLegal(Move move) const {
  if (move == Move::DeclareWin())
    return win_if_declare(*this);
  if (! move.is_ordinary_valid())
    return false;

  return isAcceptable(move) && isSafeMove(move) && ! isPawnDropCheckmate(move);
}

void osl::EffectState::
setPinOpen(Player defense){
  PieceMask pins;
  auto king = kingSquare(defense);
  if (king.isPieceStand()) {
    pin_or_open[defense] = pins;
    return;
  }
  PieceMask attack = piecesOnBoard(alt(defense));
  auto& km_side = king_visibility[defense];
  makePinOpenDir<UL>(king, pins, attack, km_side);
  makePinOpenDir<U> (king, pins, attack, km_side);
  makePinOpenDir<UR>(king, pins, attack, km_side);
  makePinOpenDir<L> (king, pins, attack, km_side);
  makePinOpenDir<R> (king, pins, attack, km_side);
  makePinOpenDir<DL>(king, pins, attack, km_side);
  makePinOpenDir<D> (king, pins, attack, km_side);
  makePinOpenDir<DR>(king, pins, attack, km_side);
  pin_or_open[defense] = pins;
}

void osl::EffectState::copyFrom(const EffectState& src)
{
#ifndef MINIMAL
  this->used_mask=src.used_mask;
#endif
  this->stand_mask=src.stand_mask;
  for(int y: board_y_range())
    for(int x: board_x_range())
      this->board[Square(x,y)]=src.board[Square(x,y)];
  this->pieces=src.pieces;

  this->pawnMask=src.pawnMask;
  this->stand_count = src.stand_count;
  this->side_to_move=src.side_to_move;
  effects.copyFrom(src.effects);
  this->pieces_onboard=src.pieces_onboard;
  this->promoted=src.promoted;
  this->pin_or_open=src.pin_or_open;
  this->king_visibility=src.king_visibility;
  this->king8infos=src.king8infos;
}

bool osl::EffectState::isSafeMove(Move move) const { return is_safe(*this, move); }
bool osl::EffectState::isCheck(Move move) const { return is_check(*this, move); }
bool osl::EffectState::isPawnDropCheckmate(Move move) const {
  return is_pawn_drop_checkmate(*this, move);
}
bool osl::EffectState::isDirectCheck(Move move) const { return is_direct_check(*this, move); }
bool osl::EffectState::isOpenCheck(Move move) const { return is_open_check(*this, move); }

void osl::EffectState::generateLegal(MoveVector& moves) const {
  moves.clear();
  moves.reserve(Move::MaxUniqMoves);
  if (inCheck()) {
    // 王手がかかっている時は防ぐ手のみを生成, 王手回避は不成も生成
    GenerateEscapeKing::generate(*this, moves);
  }
  else {
    // そうでなければ全ての手を生成
    MoveVector all_moves;
    all_moves.reserve(Move::MaxUniqMoves);
    MoveStore store(all_moves);
    move_generator::AllMoves::generate(turn(), *this, store);
    // この指手は，玉の素抜きがあったり，打歩詰の可能性があるので確認が必要
    for (auto move: all_moves) {
      if (! isSafeMove(move) || isPawnDropCheckmate(move)) {
        // std::cerr << "filter " << move << " by " << !isSafeMove(move) << isPawnDropCheckmate(move) <<'\n';
        continue;
      }
      moves.push_back(move);
    }
  }
}

bool osl::EffectState::inCheckmate() const {
  if (! inCheck())
    return false;
  MoveVector moves;
  generateLegal(moves);
  return moves.empty();
}

bool osl::EffectState::inNoLegalMoves() const {
  MoveVector moves;
  generateLegal(moves);
  if (moves.empty())
    generateWithFullUnpromotions(moves);
  return moves.empty();
}

void osl::EffectState::generateCheck(MoveVector& moves) const
{  
  moves.clear();
  using namespace osl::move_generator;
  if (! inCheck()) {
    MoveStore store(moves);
    Player player = turn();
    Square target = kingSquare(alt(turn()));
    bool has_pawn_checkmate=false;
  
    if(player==BLACK)
      AddEffect::generate<BLACK>(*this,target,store,has_pawn_checkmate);
    else
      AddEffect::generate<WHITE>(*this,target,store,has_pawn_checkmate);
  }
  else {
    MoveVector all_moves;
    GenerateEscapeKing::generate(*this, all_moves);
    for (auto move: all_moves)
      if (isCheck(move))
        moves.push_back(move);
  }
}

void osl::EffectState::generateWithFullUnpromotions(MoveVector& moves) const {
  generateLegal(moves);
  if (inCheck())
    return;
  for (int i=0, iend=moves.size(); i<iend; ++i) {
    const Move move = moves[i];
    if (move.hasIgnoredUnpromote())
      moves.push_back(move.unpromote());
  }
}

osl::Move osl::EffectState::tryCheckmate1ply() const {
  auto best_move=Move::PASS(turn());
  if (! inCheck())
    ImmediateCheckmate::hasCheckmateMove(turn(),*this,best_move);
  return best_move;
}

osl::Move osl::EffectState::findThreatmate1ply() const {
  auto best_move=Move::PASS(turn());
  if (! inCheck())
    ImmediateCheckmate::hasCheckmateMove(alt(turn()),*this,best_move);
  return best_move;
}

void osl::EffectState::
findEffect(Player P, Square target, PieceVector& out) const
{
  effect_action::StorePiece store(&out);
  forEachEffect(P, target, store);
}

template <osl::Player P>
bool osl::EffectState::
hasEffectByWithRemove(Square target,Square removed) const
{
  const Piece piece = pieceAt(removed);
  if (! piece.isPiece()) 
    return hasEffectAt<P>(target);
  if (piece.owner() == P) {
    if (hasEffectNotBy(P, piece, target))
      return true;
  }
  else {
    if (hasEffectAt(P, target))
      return true;
  }
  if (! longEffectAt(removed, P))
    return false;
  const Direction d = to_long_direction<BLACK>(to_offset32(target,removed));
  if (!is_long(d))
    return false;
  const int num=ppLongState()[piece.id()][long_to_base8(d)];
  return (! Piece::isEmptyNum(num)
	  && pieceOf(num).owner()==P);
}

namespace osl
{
  template <Player P>
  struct SafeCapture
  {
  public:
    const EffectState& state;
    Piece safe_one;
    SafeCapture(const EffectState& s) : state(s), safe_one(Piece::EMPTY()) {
    }
    void operator()(Piece effect_piece, Square target) {
      if (move_classifier::KingOpenMove::isMember<P>
	  (state, effect_piece.ptype(), effect_piece.square(), target))
	return;
      safe_one = effect_piece;
    }
  };
} // namespace osl

template <osl::Player P>
osl::Piece
osl::EffectState::safeCaptureNotByKing(Square target, Piece king) const
{
  assert(king.owner() == P);
  assert(king.ptype() == KING);
  PieceMask ignore = pin(P);
  ignore.set(king.id());
  const Piece piece = findAttackNotBy(P, target, ignore);
  if (piece.isPiece())
    return piece;
  SafeCapture<P> safe_captures(*this);
  this->forEachEffectNotBy<P>(target, king, safe_captures);
  
  return safe_captures.safe_one;
}

namespace osl
{
  template bool EffectState:: 
  hasEffectByWithRemove<BLACK>(Square, Square) const;
  template bool EffectState:: 
  hasEffectByWithRemove<WHITE>(Square, Square) const;
  template void EffectState::makeKing8Info<BLACK>();
  template void EffectState::makeKing8Info<WHITE>();

#ifndef DFPNSTATONE
  template Piece 
  EffectState::safeCaptureNotByKing<BLACK>(Square, Piece) const;
  template Piece 
  EffectState::safeCaptureNotByKing<WHITE>(Square, Piece) const;
#endif
}

// init order
// tables.cc


namespace osl {
  // table generators
  namespace {
    // boardTable.cc
    auto make_Long_Directions() {
      CArray<Direction,Offset32_SIZE> table;
      table.fill(Direction());  // Direction(0) is suitable for sentinel as others are is_long.
      for (auto dir: long_directions()) {
        const int dx=black_dx(dir), dy=black_dy(dir);
        for (int n: std::views::iota(1,9))
          table[idx(to_offset32(dx*n, dy*n))] = dir;
      }
      return table;
    }
    auto make_Basic10_Offsets() {
      CArray<Offset, Offset32_SIZE> table;
      table.fill(Offset_ZERO);
      for (auto dir: long_directions()) {
        const int dx=black_dx(dir), dy=black_dy(dir);
        Offset offset=make_offset(dx,dy);
        for (int n: std::views::iota(1,9)) 
          table[idx(to_offset32(n*dx, n*dy))]= offset;
      }
      for (auto dir: knight_directions) {
        int dx=black_dx(dir), dy=black_dy(dir);
        Offset32 offset32=to_offset32(dx,dy);
        Offset offset=make_offset(dx,dy);
        table[idx(offset32)]=offset;
        table[idx(-offset32)]= -offset;
      }
      return table;
    }
    auto make_Base8_Offsets_Rich() {
      CArray<Offset, Offset32_SIZE> table;
      table.fill(Offset_ZERO);
      for (auto dir: long_directions()) {
        const int dx=black_dx(dir), dy=black_dy(dir);
        Offset offset=make_offset(dx,dy);
        for (int n: std::views::iota(1,9))
          table[idx(to_offset32(n*dx, n*dy))]= offset;
      }
      return table;
    }

    auto make_Base8Offset() {
      CArray<signed char, OnBoard_Offset_SIZE> table;
      table.fill(0);
      for (auto dir: long_directions()) {
        const int dx=black_dx(dir), dy=black_dy(dir);
        Offset offset=make_offset(dx,dy);
        for (int n: std::views::iota(1,9))
          table[onboard_idx(make_offset(n*dx,n*dy))]= Int(offset);
      }
      return table;
    }
    auto make_Base8Direction() {
      CArray<unsigned char, OnBoard_Offset_SIZE> table;
      table.fill(Direction_INVALID_VALUE);
      for (auto dir: long_directions()) {
        const int dx=black_dx(dir), dy=black_dy(dir);
        for (int n: std::views::iota(1,9))
          table[onboard_idx(make_offset(n*dx,n*dy))]= Int(long_to_base8(dir));
      }
      return table;
    }
    static_assert(sizeof(EffectDirection) == 4, "size");
    // ptypeTable
    auto make_Effect_Table() {
      CArray2d<EffectDirection,Ptypeo_SIZE,Offset32_SIZE> table;
      table.fill();
      for(auto ptype: all_ptype_range()) {
        for (auto dir: all_directions()) {          
          if(bittest(ptype_move_direction[idx(ptype)], dir)){
            int dx=black_dx(dir), dy=black_dy(dir), pb=Int(ptype)-Ptypeo_MIN, pw=Int(ptype)-16-Ptypeo_MIN;
            Offset32 offset32=to_offset32(dx,dy);
            Offset offset = make_offset(dx,dy);
            if(is_long(dir)) {
              table[pb][idx(offset32)] = pack_long_neighbor(offset);
              table[pw][idx(-offset32)]= pack_long_neighbor(-offset);

              for(int i=2;i<9;i++){
                offset32=to_offset32(dx*i,dy*i);
                table[pb][idx(offset32)] = pack_long_far(offset);
                table[pw][idx(-offset32)]= pack_long_far(-offset);
              }
            }
            else{
              table[pb][idx(offset32)] = EffectDefinite;
              table[pw][idx(-offset32)]= EffectDefinite;
            }
          }
        }
      }
      return table;
    }
  }

  auto make_BoardMaskTable3x3() {
    CArray<BoardMask, Square::SIZE> table;
    for (int cy: board_y_range()) {
      for (int cx: board_x_range()) {
        const int min_x = std::max(1, cx - 1), max_x = std::min(9, cx + 1);
        const int min_y = std::max(1, cy - 1), max_y = std::min(9, cy + 1);
        BoardMask mask;
        mask.clear();
        for (int x: std::views::iota(min_x, max_x+1))
          for (int y: std::views::iota(min_y, max_y+1))
            mask.set(Square(x,y));
        table[Square(cx,cy)] = mask;
      }
    }
    return table;
  }
}

namespace osl {
  auto make_rng() {
    static const auto env = std::getenv("MINIOSL_DETERMINISTIC");
    static int cnt = 0;
    static std::random_device rdev;
    return std::default_random_engine(env ? (cnt++) : rdev());
  }
}

namespace osl
{
  std::array<std::default_random_engine,4> rng::rngs = {make_rng(), make_rng(), make_rng(), make_rng() };
  // NOTE: the order matters here
  const CArray<Direction,Offset32_SIZE> board::Long_Directions = make_Long_Directions();
  const CArray<Offset, Offset32_SIZE> board::Basic10_Offsets = make_Basic10_Offsets();
  const CArray<Offset, Offset32_SIZE> board::Base8_Offsets_Rich = make_Base8_Offsets_Rich();
  const CArray<signed char, OnBoard_Offset_SIZE> board::Base8_Offsets = make_Base8Offset();
  const CArray<unsigned char, OnBoard_Offset_SIZE> board::Base8_Directions = make_Base8Direction();
  // PtypeTable depends on arrays above from ex Board_Table
  const CArray2d<EffectDirection,Ptypeo_SIZE,Offset32_SIZE> board::Ptype_Effect_Table = make_Effect_Table();

  const checkmate::ImmediateCheckmateTable checkmate::Immediate_Checkmate_Table;
  const CArray<BoardMask, Square::SIZE> BoardMaskTable3x3 = make_BoardMaskTable3x3();
}

