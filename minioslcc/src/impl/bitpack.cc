#include "bitpack.h"
#include "feature.h"
#include <algorithm>
#include <iostream>
#include <cmath>

uint64_t osl::bitpack::detail::combination_id(int first, int second) {
  assert(0 <= first && first < second);
  return first + second*(second-1)/2;
}
uint64_t osl::bitpack::detail::combination_id(int first, int second, int third) {
  assert(0 <= first && first < second && second < third);
  return combination_id(first, second) + third*(third-1)*(third-2)/6;
}
uint64_t osl::bitpack::detail::combination_id(int first, int second, int third, int fourth) {
  assert(0 <= first && first < second && second < third && third < fourth);
  return combination_id(first, second, third) + fourth*(fourth-1)*(fourth-2)*(fourth-3)/24;
}

namespace osl
{
  constexpr uint64_t comb2(int n) { return n*(n-1)/2; }
  constexpr uint64_t comb4(int n) { return n*(n-1)*(n-2)*(n-3)/24; }
  std::array<uint64_t,7> encode(const int ptype_id[8][4]) {
    uint64_t done=0;
    auto renumber = [&](int id) { return uint64_t(id - std::popcount(done & (one_hot(id)-1))); };
    auto add1 = [&](int id) { auto ret = renumber(id); done |= one_hot(id); return ret; };
    auto king = add1(ptype_id[basic_idx(KING)][0])*39 + add1(ptype_id[basic_idx(KING)][1]);
    auto add2 = [&](const int ids[]) {
      int a = renumber(ids[0]);
      int b = renumber(ids[1]);
      auto ret = bitpack::detail::combination_id(a, b);
      done |= one_hot(ids[0]); done |= one_hot(ids[1]);
      return ret;
    };
    auto rook   = add2(ptype_id[basic_idx(ROOK)]);
    auto bishop = add2(ptype_id[basic_idx(BISHOP)]);

    auto add4 = [&](const int ids[]) {
      int a = renumber(ids[0]), b = renumber(ids[1]), c = renumber(ids[2]), d = renumber(ids[3]);
      assert(a>=0); 
      auto ret = bitpack::detail::combination_id(a, b, c, d);
      done |= one_hot(ids[0]); done |= one_hot(ids[1]); done |= one_hot(ids[2]); done |= one_hot(ids[3]);
      return ret;
    };
    auto gold   = add4(ptype_id[basic_idx(GOLD)]);
    auto silver = add4(ptype_id[basic_idx(SILVER)]);
    auto knight = add4(ptype_id[basic_idx(KNIGHT)]);
    auto lance  = add4(ptype_id[basic_idx(LANCE)]);
  
    if (std::popcount(done) != 40-18)
      std::domain_error("StateLabelTuple encode");
    return {king, rook, bishop, gold, silver, knight, lance };
  }
  struct B256Extended {
    uint128_t board = 0; // 81 (64+17)
    uint64_t order_lo = 0; // 57 (47+10) : 138
    uint32_t order_hi = 0; // 30 : 168
    uint64_t color = 0; // 38 (24+14) : 206
    uint64_t promote = 0; // 34 : 240
    uint32_t turn = 0; // 1
    uint32_t move = 0; // 12
    uint32_t game_result = 0; // 2
    uint32_t flip = 0; // 1
  };
  inline B256 pack(const B256Extended& code) {
    std::array<uint64_t,4> binary;
    binary[0] = code.board >> 17;
    binary[1] = (code.board & (one_hot(17)-1))<<47;
    binary[1] += code.order_lo >> 10;
    binary[2] = (code.order_lo & (one_hot(10)-1)) <<30;
    binary[2] += code.order_hi;
    binary[2] <<= 24;
    binary[2] += code.color >> 14;
    binary[3] = (code.color & (one_hot(14)-1))<<34;
    binary[3] += code.promote;
    binary[3] <<= 16;
    binary[3] += (code.turn << 15) + (code.move << 3) + (code.game_result << 1) + code.flip;
    return {binary};
  }
  inline B256Extended unpack(const std::array<uint64_t,4>& binary) {
    B256Extended code;
    code.board = binary[0];
    code.board <<= 17;
    code.board += binary[1]>>47;
    code.order_lo = ((binary[1] & (one_hot(47)-1)) << 10) + (binary[2]>>54);
    code.order_hi = (binary[2] >> 24) & (one_hot(30)-1);
    code.color = ((binary[2] & (one_hot(24)-1))<<14) + (binary[3]>>50);
    code.promote = (binary[3] >> 16) & (one_hot(34)-1);
    code.turn = (binary[3] >> 15) & 1;
    code.move = (binary[3] >> 3) & (one_hot(12)-1);
    code.game_result = (binary[3] >> 1) & 3;
    code.flip = binary[3] & 1;
    return code;
  }
  namespace
  {
    inline int code_color_id(Ptype ptype, int piece_id) {
      return ptype_has_long_move(unpromote(ptype)) ? piece_id-2 : piece_id; // skip two kings
    }
    inline int code_promote_id(Ptype ptype, int piece_id) {
      return ptype_has_long_move(unpromote(ptype)) ? piece_id-6 : piece_id; // skip two kings and four golds
    }
    inline auto one_hot128(int n) { return ((uint128_t)1) << n; }    
  }
  const int move12_dir_size = 13, move12_unpromote_offset = 5;
}

uint32_t osl::bitpack::encode12(const BaseState& state, Move move) {
  if (move == Move::Resign())      
    return move12_resign; // 0 --- inconsistent as a normal move to (1,1) by moving UL .. outside from the board
  if (move == Move::DeclareWin())
    return move12_win_declare;  // 127 --- different from normal moves due to promotion scheme
  if (move.isPass())
    return move12_pass;  // 126 --- different from normal moves due to promotion scheme
  
  auto to = move.to().blackView(state.turn());
  auto code_to = (to.x()-1)+(to.y()-1)*9; // 7bit
  if (to.y() <= 4 && move.isPromotion())
    code_to += 81; // 81 + 32 < 128
  auto code_dir_or_ptype = 0; // [0, 21)
  constexpr int dir_size = 13; // [0,UUR+3]
  if (move.isDrop()) {
    auto drop_id = basic_idx(move.ptype())-1;
    code_dir_or_ptype = drop_id + move12_dir_size;
  }
  else {
    auto from = move.from().blackView(state.turn());
    Direction dir;
    if (move.oldPtype() == KNIGHT)
      dir = to.x() > from.x() ? UUL : UUR;
    else {
      dir = base8_dir<BLACK>(from, to);
      if (to.y() > 4 && move.hasIgnoredUnpromote()) {
        assert(dir == DL || dir == D || dir == DR);
        dir = Direction(Int(dir)+move12_unpromote_offset);
      }
    }
    code_dir_or_ptype = Int(dir);
  }
  return code_dir_or_ptype*128 + code_to;
}

osl::Move osl::bitpack::decode_move12(const BaseState& state, uint32_t code) {
  if (code == move12_resign)
    return Move::Resign();
  if (code == move12_win_declare)
    return Move::DeclareWin();
  if (code == move12_pass)
    return Move::PASS(state.turn());

  auto code_to = code%128, code_dir_or_ptype = code/128;
  auto x = (code_to%9)+1, y = code_to/9 + 1;
  bool promote = false;
  if (y > 9) {
    promote = true;
    y -= 9;
    if (y > 4)
      throw std::domain_error("decode inconsistent promotion y " + std::to_string(code));
  } 
  Square to(x,y);
  if (state.turn() == WHITE)
    to = to.blackView(WHITE);      // change view
    
  if (code_dir_or_ptype >= move12_dir_size) { // drop
    auto ptype = basic_ptype[code_dir_or_ptype-move12_dir_size+1]; // 0 is for KING
    if (! state.pieceAt(to).isEmpty())
      throw std::domain_error("decode inconsistent dropto " + std::to_string(code));
    return Move(to, ptype, state.turn());
  }
  // move on board
  if (! state.pieceAt(to).canMoveOn(state.turn()))
    throw std::domain_error("decode inconsistent to " + std::to_string(code));
  Direction dir = Direction(code_dir_or_ptype);
  if (! is_basic(dir)) {
    dir = Direction(code_dir_or_ptype-move12_unpromote_offset);
    promote = true;
  }
  Offset step = to_offset(state.turn(), dir);
  Square from = to - step;
  while (state.pieceAt(from).isEmpty())
    from -= step;
  if (! state.pieceAt(from).isOnBoardByOwner(state.turn())) {
    if (code == 493 || code == 611)          // backward compatibility for a while :-(
      return Move::PASS(state.turn());
    throw std::domain_error("decode inconsistent from " + std::to_string(code)
                            +" "+to_usi(state));
  }
  auto ptype = state.pieceAt(from).ptype();
  if (promote)
    ptype = osl::promote(ptype);
  return Move(from, to, ptype, state.pieceAt(to).ptype(), promote, state.turn());
}

std::pair<int,int> osl::bitpack::detail::unpack2(uint32_t code) {
  int n2 = std::floor(std::sqrt(2.0*code))+1;
#if 0
  if (n2 == 0 || combination_id(0, n2+1) <= code)
    ++n2; 
  else
#endif
    if (combination_id(0, n2) > code)
      --n2;
  int n1 = code - combination_id(0, n2);
  return {n1, n2};
}

std::tuple<int,int,int,int> osl::bitpack::detail::unpack4(uint64_t code) {
  int n4 = std::floor(std::sqrt(std::sqrt(24.0*code)))+2;
  if (n4 == 0 || combination_id(0, 1, 2, n4+1) <= code)
    ++n4; 
  else if (combination_id(0, 1, 2, n4) > code)
    --n4;
  code -= combination_id(0, 1, 2, n4);
  int n3 = std::floor(std::cbrt(6.0*code))+1;
  if (n3 == 0 || combination_id(0, 1, n3+1) <= code)
    ++n3; 
#if 0
  else if (combination_id(0, 1, n3) > code)
    --n3;
#endif
  code -= combination_id(0, 1, n3);
  auto [n1, n2] = unpack2(code);
  return std::make_tuple(n1, n2, n3, n4);
}


osl::bitpack::B256 osl::bitpack::StateRecord256::to_bitset() const {
  B256Extended code;
  std::vector<Piece> board_pieces; board_pieces.reserve(40);
  for (int x: board_y_range()) // 1..9
    for (int y: board_y_range()) {
      auto p = state.pieceAt(Square(x,y));
      if (! p.isPiece())
        continue;
      code.board |= one_hot128((x-1)*9+(y-1));
      board_pieces.push_back(p);
    }
  int ptype_count[8] = { 0 };  // number of completed pieces
  int ptype_bid[8][4] = { 0 }; // (ptype,n-th) -> owned square id
  for (auto bi: std::views::iota(0,int(board_pieces.size()))) {
    auto p = board_pieces[bi];
    auto ptype = unpromote(p.ptype());
    // renumber
    int nth = ptype_count[basic_idx(ptype)]++;
    if (ptype == KING)
      ptype_bid[basic_idx(ptype)][idx(p.owner())] = bi;
    else if (ptype != PAWN)
      ptype_bid[basic_idx(ptype)][nth] = bi;
    
    int piece_id = nth + ptype_piece_id[idx(ptype)].first;
    if (p.ptype() == KING)
      continue;
    if (idx(p.owner()))
      code.color |= one_hot(code_color_id(p.ptype(), piece_id)); // skip two kings    
    if (p.ptype() == GOLD)
      continue;
    if (p.isPromoted())
      code.promote |= one_hot(code_promote_id(p.ptype(), piece_id)); // skip two kings and four golds
  }
  // stand
  int hp = board_pieces.size();
  auto encode_stand = [&](Player pl) {
    for (auto ptype: piece_stand_order) {
      int cnt = state.countPiecesOnStand(pl, ptype);
      for (int i: std::views::iota(0, cnt)) {
        int nth = ptype_count[basic_idx(ptype)]++;
        int piece_id = nth + ptype_piece_id[idx(ptype)].first;
        if (idx(pl))
          code.color |= one_hot(code_color_id(ptype, piece_id));
        if (ptype != PAWN) 
          ptype_bid[basic_idx(ptype)][nth] = hp++;
      }
    }
  };
  encode_stand(BLACK);
  encode_stand(WHITE);

  for (auto ptype: basic_ptype) {
    if (ptype == PAWN) continue;
    if (ptype_count[basic_idx(ptype)] != ptype_piece_count(ptype))
      throw std::domain_error("StateLabelTuple::to_bitset unexpected piece number");
  }
  auto [king_code, rook_code, bishop_code, gold_code, silver_code, knight_code, lance_code] = encode(ptype_bid);
  code.order_hi= (king_code * comb2(38) + rook_code) * comb2(36) + bishop_code;
  code.order_lo= (((gold_code * comb4(30)) + silver_code) * comb4(26) + knight_code) * comb4(22) + lance_code;
  assert(code.order_lo < (1ull<<57));
  code.turn = idx(state.turn());
  code.game_result = (unsigned int)this->result;

  code.move = encode12(state, this->next);
  code.flip = this->flipped;
  
  return pack(code);
}

void osl::bitpack::StateRecord256::restore(const B256& packed) {
  B256Extended code = unpack(packed);
  state.initEmpty();

  std::vector<Square> on_board;  
  for (int x: board_y_range())
    for (int y: board_y_range())
      if (code.board & one_hot128((x-1)*9+(y-1)))
        on_board.emplace_back(x,y);

  std::vector<Ptype> ptype_placement(on_board.size(), Ptype_EMPTY);
  auto nth_empty = [&](int n) {
    for (int i=0; i<on_board.size(); ++i) {
      if (ptype_placement[i] == Ptype_EMPTY && n-- == 0)
        return i;
    }
    return (int)on_board.size() + n;
  };

  // inverse of
  // uint32_t hi= (king_code * comb2(38) + rook_code) * comb2(36) + bishop_code;
  uint32_t code_hi = code.order_hi;
  auto king_code = code_hi / comb2(38) / comb2(36);
  state.setPiece(BLACK, on_board[king_code / 39], KING);
  ptype_placement[king_code / 39] = KING;
  int wking_bid = nth_empty(king_code % 39);
  state.setPiece(WHITE, on_board[wking_bid], KING);
  ptype_placement[wking_bid] = KING;

  int ptype_bid[8][4] = { 0 };
  auto rook = ptype_bid[basic_idx(ROOK)];
  std::tie(rook[0], rook[1]) = detail::unpack2((code_hi / comb2(36)) % comb2(38));
  auto bishop = ptype_bid[basic_idx(BISHOP)];
  std::tie(bishop[0], bishop[1]) = detail::unpack2(code_hi % comb2(36));
  
  typedef std::pair<Ptype,int> pair_t;
  uint64_t code_lo = code.order_lo;
  for (auto [ptype, n]: {std::make_pair(LANCE,22), {KNIGHT,26}, {SILVER,30}, {GOLD,34}}) {
    auto data = ptype_bid[basic_idx(ptype)];
    auto c = code_lo % comb4(n);
    std::tie(data[0], data[1], data[2], data[3]) = detail::unpack4(c);
    code_lo /= comb4(n);
  }
  
  for (auto ptype: std::array<Ptype,6>{{ROOK, BISHOP, GOLD, SILVER, KNIGHT, LANCE}}) {
    uint64_t bids = 0;
    for (int i:std::views::iota(0, ptype_piece_count(ptype))) {
      int bid = nth_empty(ptype_bid[basic_idx(ptype)][i]);
      int piece_id = i + ptype_piece_id[idx(ptype)].first;
      Player owner = players[bittest(code.color, code_color_id(ptype, piece_id))];
      if (bid >= on_board.size()) {
        state.setPiece(owner, Square::STAND(), ptype);
      }
      else {
        bool promoted = can_promote(ptype) && bittest(code.promote, code_promote_id(ptype, piece_id));
        auto pptype = promoted ? promote(ptype) : ptype;
        state.setPiece(owner, on_board[bid], pptype);
        bids |= one_hot(bid);
      }
    }
    for (auto n: to_range(bids))
      ptype_placement[n] = ptype;
  }
  // pawn
  int pawn_cnt=0;
  for (size_t i=0; i<ptype_placement.size(); ++i) {
    if (ptype_placement[i] != Ptype_EMPTY)
      continue;
    int piece_id = pawn_cnt++ + ptype_piece_id[idx(PAWN)].first;
    Player owner = players[bittest(code.color, code_color_id(PAWN, piece_id))];
    bool promoted = bittest(code.promote, code_promote_id(PAWN, piece_id));
    state.setPiece(owner, on_board[i], promoted ? PPAWN : PAWN);
  }
  // stand pawn
  while (pawn_cnt < ptype_piece_count(PAWN)) {
    int piece_id = pawn_cnt++ + ptype_piece_id[idx(PAWN)].first;
    Player owner = players[bittest(code.color, code_color_id(PAWN, piece_id))];    
    state.setPiece(owner, Square::STAND(), PAWN);
  }
  state.setTurn(players[code.turn]);
  state.initFinalize();

  this->result = GameResult(code.game_result);
  this->next = decode_move12(this->state, code.move);
  this->flipped = code.flip;
}

void osl::bitpack::StateRecord256::flip() {
  flipped ^= 1;
  state = state.rotate180();
  next = next.rotate180();
  result = osl::flip(result);
}

int osl::bitpack::append_binary_record(const MiniRecord& record, std::vector<uint64_t>& out) {
  constexpr int move_length_limit = 1<<10;
  if (record.initial_state != BaseState(HIRATE))
    throw std::domain_error("append_binary_record initial state != startpos");
  if (record.moves.size() >= move_length_limit)
    throw std::domain_error("append_binary_record length limit over "+std::to_string(record.moves.size()));
  if (record.moves.size() == 0)
    return 0;
  size_t size_at_beginning = out.size();
  std::vector<uint16_t> code_moves; code_moves.reserve(256);
  uint16_t header = (record.moves.size() << 2) + record.result;
  // std::cerr << "header " << header << ' ' << std::bitset<12>(header) << '\n';
  code_moves.push_back(header);
  EffectState state(record.initial_state);
  for (auto move: record.moves) {
    code_moves.push_back(encode12(state, move));
    state.makeMove(move);
  }
  if (record.has_winner())
    code_moves.push_back(encode12(state, record.final_move));
  
  int used = 0;
  uint64_t work = 0;
  for (auto code: code_moves) {
    if (used +12 <= 64) {
      work += uint64_t(code) << used;
      used += 12;
    }
    else {
      int avail = 64-used;
      if (avail)
        work += (code&(one_hot(avail)-1)) << used;
      out.push_back(work);
      work = code >> avail;
      used = 12-avail;
    }
  }
  if (used > 0)
    out.push_back(work);
  
  return out.size() - size_at_beginning;
}

int osl::bitpack::read_binary_record(const uint64_t *&in, MiniRecord& record) {
  auto ptr_at_beginning = in;
  uint64_t work = *in++;
  int remain = 64;
  auto retrieve = [&]() {
    if (remain >= 12) {
      auto value = work & (one_hot(12)-1); work >>= 12; remain -= 12;
      return value;
    }
    uint64_t value = work;
    work = *in++;
    value += (work & (one_hot(12-remain)-1)) << remain;
    work >>= 12-remain;
    remain = 64 - 12 + remain;
    return value;
  };
  uint16_t header = retrieve();
  // std::cerr << "header " << header << ' ' << std::bitset<12>(header) << '\n';
  record.set_initial_state(EffectState());
  const int length = header >> 2;
  record.moves.reserve(length);
  record.result = GameResult(header & 3);

  EffectState state;
  for (int cnt=0; cnt < length; ++cnt) {
    uint16_t code = retrieve();
    auto move = decode_move12(state, code);
    state.makeMove(move);
    record.append_move(move, state.inCheck());
  }
  if (record.has_winner())
    record.final_move = decode_move12(state, retrieve());

  record.settle_repetition();
  
  return in - ptr_at_beginning;
}


osl::bitpack::B320 osl::bitpack::StateRecord320::to_bitset() const {
  // before: past --- history[0], history[1],..., history[4], base.next --- future
  // after: past --- base.next, history[1],..., history[4], history[0] --- future
  // base.next need to be a legal move in base.state to apply base.to_bitset()
  // similarly, we need the corresponding state for each move encoding
  auto base = this->base;
  auto history = this->history;
  std::swap(base.next, history[0]);
  B256 base_code = base.to_bitset();
  
  // std::cerr << "enc base " << base.next << '\n';
  EffectState state(base.state);
  if (base.next.isNormal())
    state.makeMove(base.next);
  uint64_t history_code=0;
  for (int j=0; j<5; ++j) {
    int i = (j+1)%5;
    auto move = history[i];
    uint64_t code = encode12(state, move);
    history_code += code << (j*12);
    // std::cerr << "enc " <<j << ' ' << i << ' ' << move << '\n';
    if (move.isNormal())
      state.makeMove(move);
  }
  B320 ret;
  for (int i=0; i<4; ++i) ret[i] = base_code[i];
  ret[4] = history_code;
  return ret;
}

void osl::bitpack::StateRecord320::restore(const B320& binary) {
  B256 base_code{binary[0], binary[1], binary[2], binary[3]};
  base.restore(base_code);
  EffectState state(base.state);
  if (base.next.isNormal())
    state.makeMove(base.next);
  auto history_code = binary[4];
  // std::cerr << "base " << base.next << '\n';
  for (int j=0; j<5; ++j) {
    int i = (j+1)%5;    
    auto code = history_code & ((1ull << 12)-1);
    history_code >>= 12;
    auto move = decode_move12(state, code);
    history[i] = move;
    // std::cerr << i << ' ' << move << '\n';
    if (move.isNormal()) {
      if (! state.isAcceptable(move))
        throw std::domain_error("not acceptable "+to_csa(move));
      state.makeMove(move);
    }
  }
  std::ranges::remove(history, Move());
  std::swap(base.next, history[0]);
}

void osl::bitpack::StateRecord320::flip() {
  base.flip();
  for (auto &move: history)
    if (move.isNormal())
      move = move.rotate180();
}

osl::EffectState osl::bitpack::StateRecord320::make_state() const {
  EffectState state(base.state);
  for (auto move: history)
    if (move.isNormal())
      state.makeMove(move);
  return state;
}

osl::Move osl::bitpack::StateRecord320::last_move() const {
  Move ret;
  for (auto move: history)
    if (move.isNormal())
      ret = move;
  return ret;
}

void osl::bitpack::StateRecord320::
export_feature_labels(float *input, int& move_label, int& value_label, float *aux_label) const {
  MoveVector moves(history.begin(), history.end());
  while (! moves.empty() && !moves.back().isNormal())
    moves.pop_back();

  auto [state, flipped] =
    ml::write_float_feature([&](auto *out){ return ml::export_features(base.state, moves, out); },
                            ml::input_unit,
                            input);

  if (flipped)
    throw std::logic_error("StateRecord320 flip consistency"); // must already be flipped if needed
  
  // labels
  move_label = ml::policy_move_label(base.next);
  value_label = ml::value_label(base.result);

  ml::write_float_feature([&](auto *out){ ml::helper::write_np_aftermove(state, base.next, out); },
                          ml::aux_unit,
                          aux_label);
}
