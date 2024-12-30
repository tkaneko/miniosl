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

void osl::ml::board_feature(const BaseState& state, nn_input_element /*30ch*/ *planes) {
  auto dense = board_dense_feature(state);
  for (int i: std::views::iota(0,81))
    if (planes[(Int(KING)+14)*81+i] != 0)
      throw std::logic_error("planes must be zero-filled in advance");
  for (int i: std::views::iota(0,81)) {
    auto c = dense[i]+14;
    planes[c*81+i] = One;
  }
  auto edge = Int(Ptype_EDGE)+14;
  std::fill(&planes[edge*81], &planes[edge*81]+81, One);
}

void osl::ml::hand_feature(const BaseState& state, nn_input_element /*14ch*/ *planes) {
  int c = 0;
  for (auto pl: players)
    for (auto ptype: piece_stand_order) {
      int cnt = state.countPiecesOnStand(pl, ptype);
      if (cnt) {
        auto value = One * cnt / ptype_piece_count(ptype);
        std::fill(&planes[c*81], &planes[c*81+81], value);
      }
      ++c;
    }
  assert (c == 14);
}

void osl::ml::impl::fill_empty(const BaseState& state, Square src, Offset diff, nn_input_element /*1ch*/ *out) {
  if (state.pieceAt(src).isEdge())
    return;
  src += diff;
  while (state.pieceAt(src).isEmpty()) {
    out[src.index81()] = One;
    src += diff;
  }    
}

void osl::ml::impl::fill_segment(Square src, Square dst, nn_input_element *out) {
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
    out[sq.index81()] = One;
    sq += step;
  }
  if (sq.isOnBoard())
    out[sq.index81()] = One;
}

void osl::ml::lance_cover(const EffectState& state, nn_input_element /*4ch*/ *planes) {
  for (auto z: players) {
    auto pieces = state.piecesOnBoard(z);
    auto lances = (pieces & ~state.promotedPieces()).to_ullong() & piece_id_set(LANCE);
    for (int n: BitRange(lances)) {
      auto farthest = state.pieceReach(change_view(z, U), n);
      fill_segment(state.pieceOf(n), farthest, z, planes);
      fill_empty(state, farthest, to_offset(z, U), planes+(idx(z)+2)*81);
    }
  }
}

void osl::ml::bishop_cover(const EffectState& state, nn_input_element /*4ch*/ *planes) {
  for (auto z: players) {
    auto pieces = state.piecesOnBoard(z);
    auto bishops = pieces.to_ullong() & piece_id_set(BISHOP);
    for (int n: BitRange(bishops)) 
      for (auto dir: {UL, UR, DL, DR}) {
        auto farthest = state.pieceReach(dir, n);
        fill_segment(state.pieceOf(n), farthest, z, planes);
        fill_empty(state, farthest, black_offset(dir), planes+(idx(z)+2)*81);
      }
  }
}

void osl::ml::rook_cover(const EffectState& state, nn_input_element /*4ch*/ *planes) {
  for (auto z: players) {
    auto pieces = state.piecesOnBoard(z);
    auto rooks = pieces.to_ullong() & piece_id_set(ROOK);
    for (int n: BitRange(rooks)) 
      for (auto dir: {U, L, R, D}) {
        auto farthest = state.pieceReach(dir, n);
        fill_segment(state.pieceOf(n), farthest, z, planes);
        fill_empty(state, farthest, black_offset(dir), planes+(idx(z)+2)*81);
      }
  }
}

void osl::ml::king_visibility(const EffectState& state, nn_input_element /*2ch*/ *planes) {
  for (auto z: players) {    
    for (auto dir: base8_directions()) {
      fill_segment(state.kingPiece(z), state.kingVisibilityBlackView(z, dir), z, planes);
    }
  }
}

void osl::ml::impl::fill_move_trajectory(Move move, nn_input_element *out) {
  if (! move.isNormal())
    return;
  out[move.to().index81()] = One;
  auto from = move.from();
  if (! from.isPieceStand()) {
    if (move.oldPtype() == KNIGHT)
      out[from.index81()] = One;
    else
      ml::fill_segment(move.to(), from, out);
  }
}

void osl::ml::impl::fill_ptypeo(const BaseState& state, Square sq, PtypeO ptypeo, nn_input_element *out) {
  auto ptype = osl::ptype(ptypeo);
  auto color = owner(ptypeo);
  if (ptype == KNIGHT) {
    for (auto dir: { UUL, UUR }) {
      auto dst = sq + to_offset(color, dir);
      if (dst.isOnBoard())
        out[dst.index81()] = One;
    }
    return;
  }
  if (ptype == LANCE) {
    auto step = to_offset(color, U);
    for (auto dst = sq + step; state.pieceAt(dst).canMoveOn(color); dst += step) {
      out[dst.index81()] = One;
      if (! state.pieceAt(dst).isEmpty())
        break;
    }
    return;
  }  
  for (int n: BitRange(ptype_move_direction[idx(ptype)] & 255)) {
    auto dir = Direction(n);
    auto dst = sq + to_offset(color, dir);
    if (dst.isOnBoard())
      out[dst.index81()] = One;
  }
  if (unpromote(ptype) == ROOK) {
    for (auto dir: {U,D,L,R}) {
      auto step = to_offset(color, dir);
      for (auto dst = sq + step; state.pieceAt(dst).canMoveOn(color); dst += step) {   
        out[dst.index81()] = One;
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
        out[dst.index81()] = One;
        if (! state.pieceAt(dst).isEmpty())
          break;
      }
    }    
  }  
}


void osl::ml::mate_path(const EffectState& state, nn_input_element *planes) {
  auto tmove = state.findThreatmate1ply();
  if (tmove.isNormal()) {
    fill_move_trajectory(tmove, planes);
    fill_ptypeo(state, tmove.to(), tmove.ptypeO(), planes + 81);
  }
}

void osl::ml::helper::write_np_44ch(const BaseState& state, nn_input_element *ptr) {
  // board [0,29]
  ml::board_feature(state, ptr);
  // hand [30,43]
  ml::hand_feature(state, ptr+30*81);
}

void osl::ml::helper::write_np_additional(const osl::EffectState& state, bool flipped, nn_input_element *ptr) {
  // reach LBR+K 8ch
  {
    auto *lance_plane = ptr;
    auto *bishop_plane= ptr+4*81;
    auto *rook_plane  = ptr+8*81;
    auto *king_plane  = ptr+12*81;
    ml::lance_cover(state, lance_plane);
    ml::bishop_cover(state, bishop_plane);
    ml::rook_cover(state, rook_plane);
    ml::king_visibility(state, king_plane);
  }
  int c = 14;
  // pawn4
  for (auto pl: players) {
    nn_input_element p4 = One * std::min(4, state.countPiecesOnStand(pl, PAWN)) / 4;
    std::fill(ptr+c*81, ptr+(c+1)*81, p4);
    ++c;
  } 
  // flip
  std::fill(ptr+c*81, ptr+(c+1)*81, flipped*One);
  ++c;
  // check piece
  check_piece(state, ptr+c*81);
  ++c;
  // threatmate 2ch
  ml::mate_path(state, ptr+c*81);
  c += 2;

  assert(c == heuristic_channels);
}

void osl::ml::helper::write_state_features(const EffectState& state, bool flipped, nn_input_element *ptr) {
  ml::helper::write_np_44ch(state, ptr);
  ml::helper::write_np_additional(state, flipped, ptr + 9*9*ml::basic_channels);
}

void osl::ml::check_piece(const EffectState& state, nn_input_element /*1ch*/ *plane) {
  auto attack = state.effectAt(alt(state.turn()), state.kingSquare(state.turn()));
  if (attack.none())
    return;
  auto p = state.pieceOf(attack.takeOneBit());
  plane[p.square().index81()] = One;
  if (attack.none())
    return;
  // at most two pieces can make check
  p = state.pieceOf(attack.takeOneBit());
  plane[p.square().index81()] = One;
}

void osl::ml::helper::write_np_history(EffectState& state, Move last_move, nn_input_element *ptr) {
  // (1) features BEFORE the last_move
  // last_move 3ch
  {
    nn_input_element *plane = ptr;
    nn_input_element *capture = ptr + 2*81;
    
    if (last_move.isNormal()) {
      auto dst = last_move.to();
      plane[dst.index81()] = One;
      if (! last_move.isDrop())
        fill_move_trajectory(last_move, plane+81);
      if (last_move.isCapture())
        impl::fill_ptypeo(state, dst, last_move.capturePtypeO(), capture);
    }
  }
  int c = 3;
  // king 1ch
  {
    auto *plane = ptr+c*81;
    auto sq = state.kingSquare(last_move.player());
    plane[sq.index81()] = One;
    ++c;
  }

  // check piece
  check_piece(state, ptr+c*81);
  ++c;
  // threatmate 2ch
  ml::mate_path(state, ptr+c*81);
  c += 2;

  // color_of_piece 2ch
  ml::color_of_piece(state, ptr+c*81);
  c += 2;

  // (2) make_move
  state.makeMove(last_move);
  
  // (3) features AFTER the last_move
  // checkmate if capture 3ch
  if (last_move.isNormal()) {
    auto dst = last_move.to();
    auto *tplanes = ptr+c*81;
    checkmate_if_capture(state, dst, tplanes);
  }
  c += 3;

  ml::piece_changed_cover(state, ptr+c*81);
  c += 2;

  ml::cover_count(state, ptr+c*81);
  c += 2;
  
  assert(c == channels_per_history);
}

void osl::ml::checkmate_if_capture(const EffectState& state, Square sq, nn_input_element /*3ch*/ *planes) {
  if (state.countEffect(state.turn(), sq) != 1)
    return;

  auto try_capture = [&](Move capture) {
    if (! state.isAcceptable(capture))
      return false;
    EffectState copy(state);
    copy.makeMove(capture);
    Move threat = copy.tryCheckmate1ply();
    if (! threat.isNormal())
      return false;
    fill_move_trajectory(capture, planes);
    fill_move_trajectory(threat, planes+81);
    fill_ptypeo(copy, threat.to(), threat.ptypeO(), planes+2*81);
    return true;
  };
    
  Piece attack = state.pieceOf(state.effectAt(state.turn(), sq).takeOneBit());
  Move capture(attack.square(), sq, attack.ptype(), state.pieceAt(sq).ptype(), false, state.turn());
  if (try_capture(capture))
    return;
  capture = capture.promote();
  try_capture(capture);
}

void osl::ml::color_of_piece(const BaseState& state, nn_input_element /* 2ch */ *planes) {
  for (int x: board_y_range())  // 1..9
    for (int y: board_y_range()) {
      auto p = state.pieceAt(Square(x,y));
      if (! p.isEmpty())
        planes[Square::index81(x,y) + idx(p.owner())*81] = One;
    }
}

void osl::ml::piece_changed_cover(const EffectState& state, nn_input_element /* 2ch */ *planes) {
  auto both_pieces = state.changedSource();
  for (auto z: players) {
    auto pieces = both_pieces & state.piecesOnBoard(z);
    auto pp = planes + idx(z)*81;
    for (int n: pieces.toRange())
      pp[state.pieceOf(n).square().index81()] = One;
  }
}

void osl::ml::cover_count(const EffectState& state, nn_input_element /* 2ch */ *planes) {
  for (int x: board_y_range())  // 1..9
    for (int y: board_y_range()) {
      Square sq(x,y);
      planes[Square::index81(x,y)   ] = One / 4 * std::min(4, state.countEffect(BLACK, sq));
      planes[Square::index81(x,y)+81] = One / 4 * std::min(4, state.countEffect(WHITE, sq));
    }
}


void osl::ml::helper::write_np_aftermove(EffectState state, Move move, nn_input_element *ptr) {
  if (! move.isNormal() || ! state.isAcceptable(move))
    throw std::invalid_argument("unusual move");
  
  auto dst = move.to();
  state.makeMove(move);

  // 12 + 2ch --- reach LBR+K 
  {
    auto *lance_plane = ptr;
    auto *bishop_plane= ptr+4*81;
    auto *rook_plane  = ptr+8*81;
    auto *king_plane  = ptr+12*81;
    ml::lance_cover(state, lance_plane);
    ml::bishop_cover(state, bishop_plane);
    ml::rook_cover(state, rook_plane);
    ml::king_visibility(state, king_plane);
  }

  // 5ch --- trajectory, cover,
  int c=14;
  auto *plane_cover = ptr + c*81;
  ++c;
  auto *capture_cover = ptr + c*81;
  ++c;
  auto *plane_traj = ptr + c*81;
  ++c;
  auto *plane_threat = ptr + c*81; // 2ch
  c += 2;
  impl::fill_ptypeo(state, dst, move.ptypeO(), plane_cover);
  if (move.isCapture())
    impl::fill_ptypeo(state, dst, move.capturePtypeO(), capture_cover);
  plane_traj[dst.index81()] = One;
  if (! move.isDrop())
    fill_move_trajectory(move, plane_traj);

  mate_path(state, plane_threat);

  // 3ch --- checkmate-if-capt
  auto *tplanes = ptr + c*81;
  c += 3;
  checkmate_if_capture(state, dst, tplanes);

  assert(c == aux_channels);
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
      // convention -- use '-' for the current state
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
        if (ptype != KING) {
          table["black-long2-"+ptype_name] = ch+2;
          table["white-long2-"+ptype_name] = ch+3;
          ch += 4;
        }
        else
          ch += 2;
      }
      assert(ch == 58);
      table["black-pawn4"] = ch++;
      table["white-pawn4"] = ch++;
      table["flipped"] = ch++;
      table["check-piece"] = ch++;
      table["threatmate"] = ch++;
      table["threatmate-ptypeo"] = ch++;
      assert(ch == ml::board_channels);
      // convention -- use '_' for histories
      for (int i=0; i<ml::history_length; ++i) {
        auto id = std::to_string(i+1);
        auto offset = i*ml::channels_per_history;
        table["last_move_to_"+id]      = ch + 0 + offset;
        table["last_move_traj_"+id]    = ch + 1 + offset;
        table["last_move_capture_"+id] = ch + 2 + offset;

        table["last_king_"+id]         = ch + 3 + offset;
        table["check_piece_"+id]       = ch + 4 + offset;
        table["threatmate_"+id]        = ch + 5 + offset;
        table["threatmate_ptypeo_"+id] = ch + 6 + offset;
        table["pieces_black_"+id]      = ch + 7 + offset;
        table["pieces_white_"+id]      = ch + 8 + offset;
        table["dtakeback_"+id]         = ch + 9 + offset;
        table["tthreat_"+id]           = ch +10 + offset;
        table["tthreat_ptypeo_"+id]    = ch +11 + offset;
        table["cover_changed_b_"+id]   = ch +12 + offset;
        table["cover_changed_w_"+id]   = ch +13 + offset;
        table["cover_count_b_"+id]     = ch +14 + offset;
        table["cover_count_w_"+id]     = ch +15 + offset;
      }
      if (table.size() != ml::input_channels)
        throw std::logic_error("channel config inconsistency "
                               + std::to_string(table.size())
                               + " " + std::to_string(ml::input_channels));
      return table;
    }
  }
}

osl::SubRecord::SubRecord(const MiniRecord& record)
  : moves(record.moves), shogi816k_id(record.shogi816k_id),
    final_move(record.final_move), result(record.result) {
  if (! shogi816k_id
      && record.initial_state != BaseState(HIRATE))
    throw std::logic_error("unexpected initial state");
}
osl::SubRecord::SubRecord(MiniRecord&& record)
  : moves(std::move(record.moves)), shogi816k_id(record.shogi816k_id),
    final_move(record.final_move), result(record.result) {
  if (! shogi816k_id
      && record.initial_state != BaseState(HIRATE))
    throw std::logic_error("unexpected initial state");
}

osl::BaseState osl::SubRecord::initial_state() const {
  if (shogi816k_id)
    return BaseState(Shogi816K, shogi816k_id.value());
  return BaseState(HIRATE);
}

osl::BaseState osl::SubRecord::make_state(int idx) const {
  if (idx < 0 || moves.size() < idx)
    throw std::range_error("make_state: out of range");
  auto state = initial_state();
  for (int i: std::views::iota(0, idx))
    state.make_move_unsafe(moves[i]);
  return state;
}

std::pair<osl::EffectState,bool> osl::ml::export_features(BaseState base, const MoveVector& moves, nn_input_element *out, int idx) {
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

  EffectState state{ base };

  // history features depending on old state
  ml::helper::write_np_histories(state, history, out + board_channels*81);

  // features for current state
  ml::helper::write_state_features(state, flip, out);
  return {state, flip};
}


void osl::ml::helper::write_np_histories(EffectState& state, const MoveVector& history, nn_input_element *out) {
  for (int i=0; i<history.size(); ++i) {
    if (! history[i].isNormal()) 
      continue;
    auto j = history.size() - i - 1; // fill in the reverse order
    auto *ptr = out + j*9*9*ml::channels_per_history;
    ml::helper::write_np_history(state, history[i], ptr);
    // state has been updated with history[i];
  }
}

void osl::SubRecord::export_feature_labels(int idx, nn_input_element *input,
                                           int& move_label, int& value_label, nn_input_element *aux_label,
                                           MoveVector& legal_moves) const {
  if ((! (0 <= idx && idx < moves.size())) || result == InGame)
    throw std::range_error("make_state_label_of_turn: out of range"
                           " or in game " + std::to_string(idx)
                           + " < " + std::to_string(moves.size())
                           + " result " + std::to_string(result));
  auto [state, flipped] = ml::export_features(initial_state(), moves, input, idx);
  state.generateLegal(legal_moves);
  
  Move move = moves[idx];
  auto result = this->result;
  if (flipped) {
    move = move.rotate180();
    result = flip(result);
  }

  // labels
  move_label = ml::policy_move_label(move);
  value_label = ml::value_label(result);
  if (idx < 2)
    value_label = 0;

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

void osl::SubRecord::sample_feature_labels(nn_input_element *input,
                                           int& move_label, int& value_label, nn_input_element *aux_label,
                                           uint8_t *legalmove_buf,
                                           int decay, TID tid) const {
  if (shogi816k_id)
    decay = 0;
  MoveVector legal_moves;
  int idx = weighted_sampling(moves.size(), decay, tid);
  export_feature_labels(idx, input, move_label, value_label, aux_label, legal_moves);
  if (legalmove_buf)
    ml::set_legalmove_bits(legal_moves, legalmove_buf);
}

void osl::SubRecord::
sample_feature_labels_to(int offset,
                         nn_input_element *input_buf,
                         int32_t *policy_buf, float *value_buf, nn_input_element *aux_buf,
                         nn_input_element *input2_buf,
                         uint8_t *legalmove_buf, uint16_t *sampled_id_buf,
                         int decay, TID tid) const {
  if (shogi816k_id)
    decay = 0;
  int idx = weighted_sampling(moves.size(), decay, tid);
  if (sampled_id_buf)
    sampled_id_buf[offset] = idx;
  int move_label, value_label;
  MoveVector legal_moves;
  export_feature_labels(idx,
                        input_buf + offset*ml::input_unit,
                        move_label, value_label,
                        aux_buf + offset*ml::aux_unit,
                        legal_moves);
  policy_buf[offset] = move_label;
  value_buf[offset] = value_label;
  if (input2_buf)
    ml::export_features(initial_state(), moves,
                        input2_buf + offset*ml::input_unit,
                        idx+1);
  if (legalmove_buf)
    ml::set_legalmove_bits(legal_moves, legalmove_buf + offset*ml::legalmove_bs_sz);
}


void osl::ml::set_legalmove_bits(const MoveVector& legal_moves, uint8_t *buf) {
  for (auto move: legal_moves) {
    int id = ml::policy_move_label(move);
    ml::set_in_uint8bit_vector(buf, id);
  }
}


// global variable
const std::unordered_map<std::string, int> osl::ml::channel_id = osl::make_channel_id();
const int osl::ml::standard_channels = channel_id.size();
