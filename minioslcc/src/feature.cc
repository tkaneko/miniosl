#include "feature.h"
#include "record.h"
#include "impl/rng.h"

std::array<int8_t,81> osl::ml::board_dense_feature(const BaseState& state) {
  std::array<int8_t,81> board = { 0 };
  for (int x: board_y_range())  // 1..9
    for (int y: board_y_range())
      board[Square::index81(x,y)] = state.pieceAt(Square(x,y)).ptypeO();
  return board;
}

std::array<int8_t,14> osl::ml::hand_dense_feature(const BaseState& state) {
  std::array<int8_t,14> hand = { 0 };
  for (auto pl: players)
    for (auto n: std::views::iota(0,7))
      hand[n + 7*idx(pl)] = state.countPiecesOnStand(pl, piece_stand_order[n]);
  return hand;
}

std::array<int8_t,81*30> osl::ml::board_feature(const BaseState& state) {
  std::array<int8_t,81*30> board = { 0 };
  auto dense = board_dense_feature(state);
  for (int c: std::views::iota(0,30))
    for (int i: std::views::iota(0,81))
      board[c*81+i] = (c == (dense[i]+14)) || (c == (Int(Ptype_EDGE)+14));
  
  return board;
}

std::array<float,81*14> osl::ml::hand_feature(const BaseState& state) {
  std::array<float,81*14> hand = { 0 };
  int c = 0;
  for (auto pl: players)
    for (auto ptype: piece_stand_order) {
      float value = 1.* state.countPiecesOnStand(pl, ptype) / ptype_piece_count(ptype);
      std::fill(&hand[c*81], &hand[c*81+81], value);
      ++c;
    }
  assert (c == 14);
  return hand;
}

void osl::ml::impl::fill_segment(Square src, Square dst, int offset, std::array<int8_t,81*2>& out) {
  auto sq = src;
  auto x_diff = dst.x() - src.x(), y_diff = dst.y() - src.y();
  auto sign = [](int n) { return n ? n / abs(n) : n; };
  auto step = make_offset(sign(x_diff), sign(y_diff)); // base8_step fails when, e.g., (1,1) + D = (1,10)
  if (step == Offset_ZERO)     
    throw std::invalid_argument("offset 0");
  sq += step;
  while (sq != dst) {
    if (! sq.isOnBoard())
      throw std::invalid_argument("segment out of board");
    out[sq.index81()+offset] = 1;
    sq += step;
  }
  if (sq.isOnBoard())
    out[sq.index81()+offset] = 1;
}

std::array<int8_t,81*2> osl::ml::lance_cover(const EffectState& state) {
  std::array<int8_t,81*2> planes = { 0 };
  for (auto z: players) {
    auto pieces = state.piecesOnBoard(z);
    auto lances = (pieces & ~state.promotedPieces()).to_ullong() & piece_id_set(LANCE);
    for (int n: BitRange(lances)) 
      fill_segment(state.pieceOf(n), state.pieceReach(change_view(z, U), n), z, planes);
  }
  return planes;
}

std::array<int8_t,81*2> osl::ml::bishop_cover(const EffectState& state) {
  std::array<int8_t,81*2> planes = { 0 };
  for (auto z: players) {
    auto pieces = state.piecesOnBoard(z);
    auto bishops = pieces.to_ullong() & piece_id_set(BISHOP);
    for (int n: BitRange(bishops)) 
      for (auto dir: {UL, UR, DL, DR}) 
        fill_segment(state.pieceOf(n), state.pieceReach(dir, n), z, planes);
  }
  return planes;
}

std::array<int8_t,81*2> osl::ml::rook_cover(const EffectState& state) {
  std::array<int8_t,81*2> planes = { 0 };
  for (auto z: players) {
    auto pieces = state.piecesOnBoard(z);
    auto rooks = pieces.to_ullong() & piece_id_set(ROOK);
    for (int n: BitRange(rooks)) 
      for (auto dir: {U, L, R, D}) 
        fill_segment(state.pieceOf(n), state.pieceReach(dir, n), z, planes);
  }
  return planes;
}

std::array<int8_t,81*2> osl::ml::king_visibility(const EffectState& state) {
  std::array<int8_t,81*2> planes = { 0 };
  for (auto z: players) {    
    for (auto dir: base8_directions()) {
      fill_segment(state.kingPiece(z), state.kingVisibilityBlackView(z, dir), z, planes);
    }
  }
  return planes;
}

void osl::ml::impl::fill_move_trajectory(Move move, int offset, std::array<int8_t,81*2>& out) {
  if (! move.isNormal())
    return;
  out[move.to().index81() + offset] = 1;
  auto from = move.from();
  if (! from.isPieceStand()) {
    if (move.oldPtype() == KNIGHT)
      out[from.index81() + offset] = 1;
    else
      ml::fill_segment(move.to(), from, offset, out);
  }
}

void osl::ml::impl::fill_ptypeo(const BaseState& state, Square sq, PtypeO ptypeo, std::array<int8_t,81>& out) {
  auto ptype = osl::ptype(ptypeo);
  auto color = owner(ptypeo);
  if (ptype == KNIGHT) {
    for (auto dir: { UUL, UUR }) {
      auto dst = sq + to_offset(color, dir);
      if (dst.isOnBoard())
        out[dst.index81()] = 1;
    }
    return;
  }
  if (ptype == LANCE) {
    auto step = to_offset(color, U);
    for (auto dst = sq + step; state.pieceAt(dst).canMoveOn(color); dst += step) {
      out[dst.index81()] = 1;
      if (! state.pieceAt(dst).isEmpty())
        break;
    }
    return;
  }  
  for (int n: BitRange(ptype_move_direction[idx(ptype)] & 255)) {
    auto dir = Direction(n);
    auto dst = sq + to_offset(color, dir);
    if (dst.isOnBoard())
      out[dst.index81()] = 1;
  }
  if (unpromote(ptype) == ROOK) {
    for (auto dir: {U,D,L,R}) {
      auto step = to_offset(color, dir);
      for (auto dst = sq + step; state.pieceAt(dst).canMoveOn(color); dst += step) {   
        out[dst.index81()] = 1;
        if (! state.pieceAt(dst).isEmpty())
          break;
      }
    }
    return;
  }
  if (unpromote(ptype) == BISHOP) {
    for (auto dir: {UL,UR,DL,DR}) {
      auto step = to_offset(color, dir);
      for (auto dst = sq + step; state.pieceAt(dst).canMoveOn(color); dst += step) {
        out[dst.index81()] = 1;
        if (! state.pieceAt(dst).isEmpty())
          break;
      }
    }    
  }  
}


std::array<int8_t,81*2> osl::ml::mate_path(const EffectState& state) {
  std::array<int8_t,81*2> planes = { 0 }; 
  auto cmove = state.tryCheckmate1ply(), tmove = state.findThreatmate1ply();
  fill_move_trajectory(cmove,  0, planes);
  fill_move_trajectory(tmove, 81, planes);
  return planes;
}

void osl::ml::helper::write_np_44ch(const BaseState& state, float *ptr) {
  // board [0,29]
  auto board = ml::board_feature(state);
  std::ranges::copy(board, ptr);
  // hand [30,43]
  auto hand = ml::hand_feature(state);
  std::ranges::copy(hand, ptr+30*81);
}

void osl::ml::helper::write_np_additional(const osl::EffectState& state, bool flipped, float *ptr) {
  // reach LBR+K 8ch
  {
    std::array<int8_t,81*2> lance_plane = ml::lance_cover(state), bishop_plane = ml::bishop_cover(state),
      rook_plane = ml::rook_cover(state), king_plane = ml::king_visibility(state);    
    std::ranges::copy(lance_plane,  ptr);
    std::ranges::copy(bishop_plane, ptr+2*81);
    std::ranges::copy(rook_plane,   ptr+4*81);
    std::ranges::copy(king_plane,   ptr+6*81);
  }
  // checkmate, threatmate
  {
    std::array<int8_t,81*2> th_plane = ml::mate_path(state);
    std::ranges::copy(th_plane, ptr+8*81);
  }
  // pawn4
  int c = 10;
  for (auto pl: players) {
    float p4 = std::min(4, state.countPiecesOnStand(pl, PAWN)) / 4.0;
    std::fill(ptr+c*81, ptr+(c+1)*81, p4);
    ++c;
  } 
  // flip
  std::fill(ptr+c*81, ptr+(c+1)*81, flipped);
  ++c;
  assert(c == heuristic_channels);
}

void osl::ml::helper::write_state_features(const EffectState& state, bool flipped, float *ptr) {
  ml::helper::write_np_44ch(state, ptr);
  ml::helper::write_np_additional(state, flipped, ptr + 9*9*ml::basic_channels);
}

void osl::ml::helper::write_np_history(const BaseState& state, Move last_move, float *ptr) {
  int c = 0;                    // ptr must be adusted in advance
  // last_move
  {
    std::array<int8_t,81*2> plane = {0};
    std::array<int8_t,81> capture = {0};
    if (last_move.isNormal()) {
      auto dst = last_move.to();
      plane[dst.index81()] = 1;
      if (! last_move.isDrop())
        fill_move_trajectory(last_move, 81, plane);
      if (last_move.isCapture())
        impl::fill_ptypeo(state, dst, last_move.capturePtypeO(), capture);
    }
    std::ranges::copy(plane, ptr+c*81);
    c += 2;
    std::ranges::copy(capture, ptr+c*81);
    ++c;
  }
#if 0  
  // king
  {
    auto *plane = ptr+c*81;
    auto sq = state.kingSquare(last_move.player());
    std::fill(plane, plane+81, 0);
    plane[sq.index81()] = 1;
    ++c;
  }
#endif  
}

void osl::ml::helper::write_np_aftermove(EffectState state, Move move, float *ptr) {
  if (! move.isNormal() || ! state.isAcceptable(move))
    throw std::invalid_argument("unusual move");
  
  auto dst = move.to();
  state.makeMove(move);

  // 8ch --- reach LBR+K 
  {
    std::array<int8_t,81*2> lance_plane = ml::lance_cover(state), bishop_plane = ml::bishop_cover(state),
      rook_plane = ml::rook_cover(state), king_plane = ml::king_visibility(state);    
    std::ranges::copy(lance_plane,  ptr);
    std::ranges::copy(bishop_plane, ptr+2*81);
    std::ranges::copy(rook_plane,   ptr+4*81);
    std::ranges::copy(king_plane,   ptr+6*81);
  }

  // 4ch --- trajectory, cover,
  std::array<int8_t,81*2> plane_traj_threat = {0};
  std::array<int8_t,81> plane_cover = {0}, capture_cover = {0};
  impl::fill_ptypeo(state, dst, move.ptypeO(), plane_cover);
  if (move.isCapture())
    impl::fill_ptypeo(state, dst, move.capturePtypeO(), capture_cover);
  plane_traj_threat[dst.index81()] = 1;
  if (! move.isDrop())
    fill_move_trajectory(move, 0, plane_traj_threat);

  auto tmove = state.findThreatmate1ply();
  fill_move_trajectory(tmove, 81, plane_traj_threat);

  int c=8;
  std::ranges::copy(plane_cover, ptr + c*81);
  ++c;
  std::ranges::copy(capture_cover, ptr + c*81);
  ++c;
  std::ranges::copy(plane_traj_threat, ptr + c*81);
  c += 2;
}


namespace osl {
  namespace ml {
    constexpr int drop_offset = 7, direction_offset = 10;
  }
}
int osl::ml::policy_move_label(Move move) {
  if (! move.isNormal())        // todo kachi?
    throw std::invalid_argument("unexpected move label");
  if (move.player() == WHITE)
    move = move.rotate180();
  
  auto dst = move.to();
  auto index = dst.index81();
  if (move.isDrop())
    return index + (idx(move.ptype()) - idx(GOLD))*81;
  auto src = move.from();
  Direction dir;
  if (move.oldPtype() == KNIGHT)
    dir = dst.x() > src.x() ? UUL : UUR;
  else 
    dir = base8_dir<BLACK>(src, dst);
  return index + drop_offset*81 + idx(dir)*81 + (move.isPromotion() ? direction_offset*81 : 0);
}

osl::Move osl::ml::decode_move_label(int code, const BaseState& state) {
  auto color = state.turn(); // todo kachi?
  auto dst = Square::from_index81(code % 81);
  if (color == WHITE)
    dst = dst.rotate180();
  code /= 81;
  if (code < drop_offset) {
    if (! state.pieceAt(dst).isEmpty())
      throw std::domain_error("drop on piece");
    auto ptype = Ptype(code + idx(GOLD));
    return Move(dst, ptype, color);
  }
  code -= drop_offset;
  bool promotion = code >= direction_offset;
  if (promotion)
    code -= direction_offset;
  auto dir = Direction(code);
  Offset step = to_offset(color, dir);
  Square src = dst - step;
  while (state.pieceAt(src).isEmpty())
    src -= step;
  if (! state.pieceAt(src).isOnBoardByOwner(color))
    throw std::domain_error("inconsistent policy move label" + std::to_string(code));
  auto ptype = state.pieceAt(src).ptype();
  if (promotion)
    ptype = promote(ptype);
  return Move(src, dst, ptype, state.pieceAt(dst).ptype(), promotion, color);  
}

namespace osl {
  namespace {
    auto lc(std::string s) {
      std::transform(s.begin(), s.end(), s.begin(),
                     [](auto c){ return std::tolower(c); });
      return s;
    }
    auto make_channel_id() {
      std::unordered_map<std::string, int> table;
      // make sure depending only on constexpr objs
      for (auto ptype: piece_ptype) {
        auto b = newPtypeO(BLACK, ptype), w = newPtypeO(WHITE, ptype);
        auto ptype_name = lc(ptype_en_names[idx(ptype)]);
        table["black-"+ptype_name] = b+14;
        table["white-"+ptype_name] = w+14;
      }
      table["empty"] = 14;
      table["one"] = 15;
      for (int id: std::views::iota(0, (int)piece_stand_order.size())) {
        auto ptype = piece_stand_order[id];
        auto ptype_name = lc(ptype_en_names[idx(ptype)]);
        table["black-hand-"+ptype_name] = id+30;
        table["white-hand-"+ptype_name] = id+37;
      }
      int ch = 44;
      for (auto ptype: { LANCE, BISHOP, ROOK, KING }) {
        auto ptype_name = lc(ptype_en_names[idx(ptype)]);
        table["black-long-"+ptype_name] = ch;
        table["white-long-"+ptype_name] = ch+1;
        ch += 2;
      }
      assert(ch == 52);
      table["checkmate"] = 52;
      table["threatmate"] = 53;
      table["black-pawn4"] = 54;
      table["white-pawn4"] = 55;
      table["flipped"] = 56;
      for (int i=0; i<ml::history_length; ++i) {
        auto id = std::to_string(i+1);
        auto offset = i*ml::channels_per_history;
        table["last_move_to_"+id] = 57 + offset;
        table["last_move_traj_"+id] = 58 + offset;
        table["last_move_capture_"+id] = 59 + offset;
        // table["last_king_"+id] = 60 + offset;
      }
      return table;
    }
  }
}

osl::SubRecord::SubRecord(const MiniRecord& record) : moves(record.moves), final_move(record.final_move), result(record.result) {
  if (record.initial_state != BaseState(HIRATE))
    throw std::logic_error("unexpected initial state");
}

osl::BaseState osl::SubRecord::make_state(int idx) const {
  if (idx < 0 || moves.size() < idx)
    throw std::range_error("make_state: out of range");
  BaseState state(HIRATE);
  for (int i: std::views::iota(0, idx))
    state.make_move_unsafe(moves[i]);
  return state;
}

std::pair<osl::EffectState,bool> osl::ml::export_features(BaseState base, const MoveVector& moves, float *out, int idx) {
  if (idx < 0)
    idx = moves.size();
  if (idx > moves.size())
    throw std::domain_error("idx out of range");
  
  const int H = ml::history_length;
  MoveVector history(H, Move());
  const int history_length = std::min(H, idx);

  // fill history if available
  for (int i: std::views::iota(0, history_length))
    history.at(history.size()-1-i) = moves.at(idx-1-i);

  // make a base state just before the history
  for (int i: std::views::iota(0, idx - history_length))
    base.make_move_unsafe(moves[i]);

  auto turn = (history_length == 0) ? base.turn() : alt(history.back().player());
  bool flip = (turn == WHITE);
  if (flip) {
    base = base.rotate180();
    rotate180(history);
  }

  // history features depending on old state
  const auto offset = 9*9*(ml::basic_channels + ml::heuristic_channels);
  ml::helper::write_np_histories(base, history, out+offset);

  EffectState state{ base };
  // features for current state
  ml::helper::write_state_features(state, flip, out);
  return {state, flip};
}


void osl::ml::helper::write_np_histories(BaseState& base, const MoveVector& history, float *out) {
  for (int i=0; i<history.size(); ++i) {
    auto *ptr = out + i*9*9*ml::channels_per_history;
    if (! history[i].isNormal()) {
      std::fill(ptr, ptr + 9*9*ml::channels_per_history, 0);
      continue;
    }
    ml::helper::write_np_history(base, history[i], ptr);
    base.make_move_unsafe(history[i]);
  }
}

void osl::SubRecord::export_feature_labels(int idx, float *input, int& move_label, int& value_label, float *aux_label) const {
  if ((! (0 <= idx && idx < moves.size())) || result == InGame)
    throw std::range_error("make_state_label_of_turn: out of range"
                           " or in game " + std::to_string(idx)
                           + " < " + std::to_string(moves.size())
                           + " result " + std::to_string(result));
  auto [state, flipped] = ml::export_features(BaseState(HIRATE), moves, input, idx);
  Move move = moves[idx];
  auto result = this->result;
  if (flipped) {
    move = move.rotate180();
    result = flip(result);
  }

  // labels
  move_label = ml::policy_move_label(move);
  value_label = ml::value_label(result);
  ml::helper::write_np_aftermove(state, move, aux_label);
}

int osl::SubRecord::weighted_sampling(int limit, int N, TID tid) {
  // weigted sampling
  //
  // for usual cases of limit > 11 (=N)
  // - move P1, P2: 2^{-10}
  // - move Pn (3 <= n <= 11): 2*Pn-1
  // - move Pn (12 <= n): 1
  //
  // Rationale: In MZ, the latest 10^6 game records are kept, and
  // - for each 2048 (bath size) x 1000 (step) positions, a new generation is generated.
  // - If uniform, there are 10k-20k of the initial position (apparently too much)
  // - The weight of 2^{-10} reduces the number to about 10-20,
  //   reasonable for keeping the average win ratio stable.
  auto& rng = rngs.at(idx(tid));
  if (limit-1 > N) {
    std::uniform_int_distribution<> dist(N, limit-1); // inclusive, move.size()-1 at most
    int idx = dist(rng);
    if (idx > N)
      return idx;
  }
  std::uniform_real_distribution<> r01(0, 1);
  double p = r01(rng);
  int idx = std::min(N, limit-1);
  while (idx > 0 && p < 0.5) {
    --idx;
    p *= 2;
  }
  return idx;  
}

void osl::SubRecord::sample_feature_labels(float *input, int& move_label, int& value_label, float *aux_label,
                                           int default_decay, TID tid) const {
  int idx = weighted_sampling(moves.size(), default_decay, tid);
  export_feature_labels(idx, input, move_label, value_label, aux_label);
}

// global variable
const std::unordered_map<std::string, int> osl::ml::channel_id = osl::make_channel_id();
const int osl::ml::standard_channels = channel_id.size();
