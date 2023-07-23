#include "hash.h"
#include "record.h"
#include <random>
#include <unordered_map>
#include <iostream>
#include <sstream>

namespace osl {
  namespace {
    auto hash_code_initializer() {
      std::mt19937_64 rng(2014'0517'4548ull);
      std::array<uint64_t, 81*32> array;
      for (auto &e: array)
        e = rng() & ~(1ull);
      return array;
    }
  }
  static_assert(sizeof(HashStatus) == sizeof(uint64_t)*2);
}

const std::array<uint64_t, 81*32> osl::hash_code_on_board_piece = osl::hash_code_initializer();

uint64_t osl::zobrist_hash_of_board(const BaseState& state) {
  uint64_t ret = 0;
  for (int x: board_x_range()) {
    for (int y: board_y_range()) {
      auto p = state.pieceAt(Square(x,y));
      if (p.isPiece())
        ret ^= HashStatus::code(p.square(), p.ptypeO());
    }
  }
  ret += idx(state.turn());
  return ret;
}

osl::BasicHash osl::make_move(const BasicHash& code, Move move) {
  auto [board_hash, black_stand_code] = code;
  auto src = move.from(), dst = move.to();
  auto color = move.player();
  // board
  board_hash ^= HashStatus::code(dst, move.ptypeO());
  if (! src.isPieceStand()) {
    board_hash ^= HashStatus::code(src, move.oldPtypeO());
    auto capture = move.capturePtype();
    if (capture != Ptype_EMPTY)
      board_hash ^= HashStatus::code(dst, newPtypeO(alt(color), capture));
  }
  board_hash ^= 1ull;
  // stand
  PieceStand black_stand(black_stand_code);
  if (color == BLACK) {
    if (move.isCapture())
      black_stand.add(unpromote(move.capturePtype()));
    if (move.isDrop())
      black_stand.sub(move.ptype());
  }
  return {board_hash, black_stand.to_uint()};
}


osl::HashStatus osl::HashStatus::new_zero_history(Move moved, bool new_in_check) const {
  auto [board, stand] = make_move({board_hash, black_stand.to_uint()}, moved);
  HashStatus ret = *this;
  ret.board_hash = board;
  ret.black_stand = PieceStand(stand);
  ret.history = HistoryStatus();
  // supp --- incremental update
  auto color = moved.player();
  if (moved.ptype() == KING) {
    auto dst = moved.to();
    if (color == BLACK)
      ret.supp.black_king = dst.index81();
    else
      ret.supp.white_king = dst.index81();
  }
  ret.supp.turn = idx(alt(color));
  ret.supp.in_check = new_in_check;
  return ret;
}

std::ostream& osl::operator<<(std::ostream& os, const osl::HashStatus& code) {
  return os << "hash(board " << code.board_hash << ' ' << code.black_stand
            << " turn " << code.supp.turn
            << " incheck " << code.supp.in_check << " kings "
            << code.supp.black_king << ' ' << code.supp.white_king << ")";
}

int osl::consecutive_in_check(const std::vector<HashStatus>& history, int now) {
  int count = 0;
  while (now >= 0 && history[now].in_check()) {
    now -= 2;
    ++count;
  }
  return count;
}



namespace osl
{
  static_assert(sizeof(unsigned int)*/*CHARBITS*/8>=32, "PieceStand");

  const CArray<unsigned char,Ptype_MAX+1> PieceStand::shift
  = {{ 0,0,0,0,0,0,0,0, 28, 24, 18, 14, 10, 6, 3, 0, }};
  const CArray<unsigned char,Ptype_MAX+1> PieceStand::mask
  = {{ 0,0,0,0,0,0,0,0, (1<<2)-1, (1<<3)-1, (1<<5)-1, (1<<3)-1, (1<<3)-1, (1<<3)-1, (1<<2)-1, (1<<2)-1 }};
}

osl::PieceStand::
PieceStand(Player pl, const BaseState& state)
  : flags(0)
{
  for (Ptype ptype: piece_stand_order)
    add(ptype, state.countPiecesOnStand(pl, ptype));
}

std::string osl::PieceStand::to_csa(Player color) const {
  std::ostringstream ss;
  if (! any())
    return "";
      
  ss << "P" << osl::to_csa(color);
  for (Ptype ptype: piece_stand_order) 
    for (unsigned int j=0; j<get(ptype); ++j) 
      ss << "00" << osl::to_csa(ptype);
  ss << "\n";
  return ss.str();
}

bool osl::PieceStand::canAdd(Ptype type) const
{
  const auto [l, r] = ptype_piece_id[Int(type)];
  const int max = l-r;
  assert(max >= 0);
  return (static_cast<int>(get(type)) != max);
}

void osl::PieceStand::tryAdd(Ptype type)
{
  if (canAdd(type))
    add(type);
}

bool osl::PieceStand::atMostOneKind() const
{
  return std::popcount(to_uint()) <= 1;
}

#ifndef MINIMAL
bool osl::PieceStand::
carryUnchangedAfterAdd(const PieceStand& original, const PieceStand& other) const
{
  if (original.testCarries() == testCarries())
    return true;
  std::cerr << original << " + " << other << " = " << *this << "\n";
  return false;
}

bool osl::PieceStand::
carryUnchangedAfterSub(const PieceStand& original, const PieceStand& other) const
{
  if (original.testCarries() == testCarries())
    return true;
  std::cerr << original << " - " << other << " = " << *this << "\n";
  return false;
}

std::ostream& osl::operator<<(std::ostream& os, osl::PieceStand stand)
{
  os << "(stand";
  for (Ptype ptype: piece_stand_order)
  {
    os << ' ' << stand.get(ptype);
  }
  return os << ")";
}
#endif



osl::HistoryTable::HistoryTable() {
}

osl::HistoryTable::~HistoryTable() {
}

osl::GameResult osl::HistoryTable::add(int state_number, HashStatus& now,
                                       const std::vector<HashStatus>& history) {
  GameResult game_status = InGame;
  auto& entry = table_t::operator[]({now.board_hash, now.black_stand});
  int past_id = entry.empty() ? -1 : entry.back();
  if (past_id >= 0) {
    const auto& repeat_from = history[past_id];
    now.history.count = repeat_from.history.count + 1;
    if (now.history.count == 3) { // starting from zero, this is the fourth occurrence
      game_status = Draw;
      if (now.in_check()) {
        int check_count = consecutive_in_check(history, state_number);
        int duration = state_number - entry.front();
        // std::cerr << "dc " << duration << ' ' << check_count << '\n';
        if (duration <= (check_count-1) * 2)
          game_status = now.turn() == BLACK ? BlackWin : WhiteWin;
      }
      else if (state_number > 0 && history[state_number-1].in_check())  {
        int check_count = consecutive_in_check(history, state_number-1);
        int duration = state_number - entry.front();
        // std::cerr << "de " << duration << ' ' << check_count << '\n';
        if (duration <= check_count * 2)
          game_status = now.turn() == BLACK ? WhiteWin : BlackWin;
      }
    }
    if (! now.is_repeat_of(repeat_from))
      throw std::domain_error("hash collision");
    if ((state_number - past_id) % 2 != 0) // same side to move
      throw std::logic_error("odd length repeat");
    now.history.prev_dist = (state_number - past_id) / 2;
  }
  entry.emplace_back(state_number);
  return game_status;
}
