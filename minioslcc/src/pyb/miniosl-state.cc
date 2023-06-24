#include "pyb/miniosl.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/operators.h>
#include "state.h"
#include "record.h"
#include "impl/bitpack.h"
#include "impl/more.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <thread>

// namespace py = pybind11;

namespace pyosl {
  using namespace osl;
  Move read_japanese_move(const EffectState& state, std::u8string move, Square last_to=Square());
    
  /** pieces on board as numpy array */
  py::array_t<int8_t> to_np(const BaseState& state);
  /** hand pieces as numpy array */
  py::array_t<int8_t> to_np_hand(const BaseState& state);
  /** squares covered by pieces as numpy array */
  py::array_t<int8_t> to_np_cover(const EffectState& state);

  const int basic_channels = 44, ext_channels = 10;
  namespace helper {
    void write_np_44ch(const BaseState& state, float *);
    void write_np_additional(const EffectState& state, float *);
  }
  /** a simple set of state features including board and hands */
  py::array_t<float> to_np_44ch(const BaseState& state);
  /** batch version of to_np_44ch */
  std::pair<py::array_t<float>, py::array_t<int8_t>>
  to_np_batch_44ch(const std::vector<std::array<uint64_t,4>>& batch);
  py::array_t<float> to_np_stdch(const EffectState& state);

  /** pack into 256bits */
  py::array_t<uint64_t> to_np_pack(const BaseState& state);
  std::pair<MiniRecord, int> unpack_record(py::array_t<uint64_t> code_seq);
}


void pyosl::init_state_np(py::module_& m) {
  // define the base class prior to the main state class
  typedef osl::BaseState base_t;
  py::class_<base_t>(m, "BaseState", py::dynamic_attr())
    .def("turn", &base_t::turn, "player to move")
    .def("piece_at", &base_t::pieceAt, py::arg("square"), "a piece at given square")
    .def("piece", &base_t::pieceOf, py::arg("id"))
    .def("count_hand", &base_t::countPiecesOnStand)
    .def("king_square", [](const base_t& s, osl::Player P) { return s.kingSquare(P); })
    .def("to_usi", [](const base_t &s) { return osl::to_usi(s); })
    .def("to_csa", [](const base_t &s) { return osl::to_csa(s); })
    .def("hash_code", &osl::hash_code, "64bit int for board and 32bit for (black) hand pieces")
    .def("to_np", &pyosl::to_np, "pieces on board as numpy array")
    .def("to_np_hand", &pyosl::to_np_hand, "pieces on hand as numpy array")
    .def("to_np_44ch", &pyosl::to_np_44ch, "a simple set of state features including board and hands")
    .def("to_np_pack", &pyosl::to_np_pack, "pack into 256bits")
    .def("__repr__", [](const base_t &s) {
      return "<BaseState '" + osl::to_usi(s) + "'>";
    })
    .def("__str__", [](const base_t &s) { return osl::to_csa(s); })
    .def(py::self == py::self)
    .def(py::self != py::self)
    .def("__copy__",  [](const base_t& s) { return base_t(s);})
    .def("__deepcopy__",  [](const base_t& s) { return base_t(s);})
    ;
  // state
  typedef osl::EffectState state_t;
  py::class_<state_t, base_t>(m, "State", py::dynamic_attr(),
                      "shogi state = board position + pieces in hand (mochigoma)")
    .def(py::init())
    .def(py::init<const state_t&>())
    .def(py::init<const osl::BaseState&>())
    .def("reset", &state_t::copyFrom)
    .def("genmove", [](const state_t &s) {
      osl::MoveVector moves;
      s.generateLegal(moves);
      return moves;
    }, "genmove suitable for most cases (some moves are excluded)")
    .def("genmove_full", [](const state_t &s) {
      osl::MoveVector moves;
      s.generateWithFullUnpromotions(moves);
      return moves;
    }, "genenerate full moves")
    .def("genmove_check", [](const state_t &s) {
      osl::MoveVector moves;
      s.generateCheck(moves);
      return moves;
    }, "generate check moves only")
    .def("make_move", &state_t::make_move)
    .def("make_move", [](state_t &s, osl::Move move) {
      if (! s.isLegal(move) || ! s.isAcceptable(move))
        throw std::domain_error("move error "+osl::to_csa(move));
      s.makeMove(move);
    })
    .def("make_move_pass", &state_t::makeMovePass)
    .def("__repr__", [](const state_t &s) {
      return "<State '" + osl::to_usi(s) + "'>";
    })
    .def("__str__", [](const state_t &s) { return osl::to_csa(s); })
    .def("count_cover", py::overload_cast<osl::Player,osl::Square>(&state_t::countEffect, py::const_),
         "the number of pieces reachable to given square")
    .def("pieces_cover", [](const state_t& s, osl::Player P, osl::Square target) {
      return (s.piecesOnBoard(P)&s.effectAt(target)).to_ullong();
    }, "the bitset of piece-ids reachable to given square")
    .def("in_check", py::overload_cast<>(&state_t::inCheck, py::const_))
    .def("in_checkmate", py::overload_cast<>(&state_t::inCheckmate, py::const_))
    .def("to_move", &state_t::to_move, "parse and return move")
    .def("read_japanese_move", &pyosl::read_japanese_move,
         py::arg("move"), py::arg("last_to")=Square(),
         "parse and return move")
    .def("is_legal", &state_t::isLegal)
    .def("to_np_cover", &pyosl::to_np_cover, "squares covered by pieces as numpy array")
    .def("encode_move", [](const state_t& s, osl::Move m) { return osl::bitpack::encode12(s, m); },
         "compress move into 12bits uint")
    .def("decode_move", [](const state_t& s, uint32_t c) { return osl::bitpack::decode_move12(s, c); },
         "uncompress move from 12bits uint")
    .def("try_checkmate_1ply", &state_t::tryCheckmate1ply, "try to find a checkmate move")
    .def("__copy__",  [](const state_t& s) { return state_t(s);})
    .def("__deepcopy__",  [](const state_t& s) { return state_t(s);})
    .def("to_np_stdch", &pyosl::to_np_stdch, "standard set of features features")
  ;

  // functions depends on np
  m.def("unpack_record", &pyosl::unpack_record, "read record from np.array encoded by MiniRecord.pack_record");
  m.def("to_np_batch_44ch", &pyosl::to_np_batch_44ch, "batch conversion of to_np_44ch");
}

osl::Move pyosl::read_japanese_move(const EffectState& state, std::u8string move, Square last_to) {
  try {
    return kanji::to_move(move, state, last_to);
  }
  catch (std::exception& e) {
  }
  return Move::PASS(state.turn());
}

py::array_t<int8_t> pyosl::to_np(const BaseState& state) {
  auto feature = py::array_t<int8_t>(9*9);
  auto buffer = feature.request();
  auto ptr = static_cast<int8_t*>(buffer.ptr);
  for (int y: board_y_range())
    for (int x: board_x_range())
      ptr[Square(x,y).index81()] = state.pieceAt(Square(x,y)).ptypeO();
  return feature;
}

py::array_t<int8_t> pyosl::to_np_hand(const BaseState& state) {
  const int N = piece_stand_order.size();
  auto feature = py::array_t<int8_t>(N*2);
  auto buffer = feature.request();
  auto ptr = static_cast<int8_t*>(buffer.ptr);
  for (auto pl: players)
    for (auto n: std::views::iota(0,N))
      ptr[n + N*idx(pl)] = state.countPiecesOnStand(pl, piece_stand_order[n]);
  return feature;
}

py::array_t<int8_t> pyosl::to_np_cover(const EffectState& state) {
  auto feature = py::array_t<int8_t>(9*9*2);
  auto buffer = feature.request();
  auto ptr = static_cast<int8_t*>(buffer.ptr);
  for (auto pl: players)
    for (int y: board_y_range())
      for (int x: board_x_range())
        ptr[(y-1)*9+(x-1)+81*idx(pl)] = state.countEffect(pl, Square(x,y));
  return feature;
}

py::array_t<uint64_t> pyosl::to_np_pack(const BaseState& state) {
  auto packed = py::array_t<uint64_t>(4);
  auto buffer = packed.request();
  auto ptr = static_cast<uint64_t*>(buffer.ptr);
  osl::StateRecord256 instance {state};
  auto bs = instance.to_bitset();
  for (int i: std::views::iota(0,4))
    ptr[i] = bs[i];
  return packed;
}

std::pair<osl::MiniRecord, int> pyosl::unpack_record(py::array_t<uint64_t> code_seq) {
  auto buf = code_seq.request();
  auto ptr = static_cast<const uint64_t*>(buf.ptr);
  MiniRecord record;
  auto n = bitpack::read_binary_record(ptr, record);
  return {record, n};
}

py::array_t<float> pyosl::to_np_44ch(const osl::BaseState& state) {
  /*  - 14 for white pieces: [ppawn, plance, pknight, psilver, pbishop, prook,
   *    king, gold, pawn, lance, knight, silver, bishop, rook]
   *  - 2 for empty and ones
   *  - 14 for black pieces
   *  - 14 for hand pieces
   */
  auto feature = py::array_t<float>(9*9*44);
  auto buffer = feature.request();
  auto ptr = static_cast<float_t*>(buffer.ptr);
  helper::write_np_44ch(state, ptr);
  return feature;
}

void pyosl::helper::write_np_44ch(const osl::BaseState& state, float *ptr) {
  // board [0,29]
  std::array<int,81> board = { 0 };
  for (int x: board_y_range())  // 1..9
    for (int y: board_y_range())
      board[Square::index81(x,y)] = state.pieceAt(Square(x,y)).ptypeO();
  for (int c: std::views::iota(0,30))
    for (int i: std::views::iota(0,81))
      ptr[c*81+i] = c == (board[i]+14) || c == (Int(Ptype_EDGE)+14);
  // hand
  int c = 30;
  for (auto pl: players)
    for (auto n: std::views::iota(0, (int)piece_stand_order.size())) {
      std::fill(&ptr[c*81], &ptr[c*81+81], state.countPiecesOnStand(pl, piece_stand_order[n]));
      ++c;
    }
  assert (c == 44);
}

std::pair<py::array_t<float>, py::array_t<int8_t>>
pyosl::to_np_batch_44ch(const std::vector<std::array<uint64_t,4>>& batch) {
  auto batch_feature = py::array_t<float_t>(batch.size()*9*9*44);
  auto buffer = batch_feature.request();
  auto ptr = static_cast<float_t*>(buffer.ptr);
  auto batch_label = py::array_t<int8_t>(batch.size());
  auto label_buffer = batch_label.request();
  auto label_ptr = static_cast<int8_t*>(label_buffer.ptr);

  auto convert = [&](int l, int r) {
    for (int i=l; i<r; ++i) {
      const auto& binary = batch[i];
      osl::StateRecord256 obj;
      obj.restore(binary);
      helper::write_np_44ch(obj.state, &ptr[i*9*9*44]);
      label_ptr[i] = obj.next.to().index81();
    }
  };
  int c = batch.size() / 2;
  std::thread t1(convert, 0, c);
  std::thread t2(convert, c, batch.size());
  t1.join();
  t2.join();
    
  return {batch_feature, batch_label};
}

void pyosl::helper::write_np_additional(const osl::EffectState& state, float *ptr) {
  // reach LBR+K 8ch
  std::array<int,81*2> lance_plane = { 0 }, bishop_plane = { 0 }, rook_plane = { 0 }, king_plane = { 0 };
  auto fill_segment = [&](Piece p, Square dst, Player owner, std::array<int,81*2>& out, int offset=0) {
    auto sq = p.square();
    auto step = base8_step(dst, sq);
    if (step == Offset_ZERO)
      throw std::invalid_argument("offset 0");
    sq += step;
    while (sq != dst) {
      if (! sq.isOnBoard())
        throw std::invalid_argument("segment out of board");
      out[sq.index81()+idx(owner)*81] = 1;
      sq += step;
    }
    if (sq.isOnBoard())
      out[sq.index81()+idx(owner)*81] = 1;
  };
    
  for (auto z: players) {
    auto pieces = state.piecesOnBoard(z);
    auto lances = (pieces & ~state.promotedPieces()).to_ullong() & piece_id_set(LANCE);
    for (int n: BitRange(lances)) 
      fill_segment(state.pieceOf(n), state.pieceReach(change_view(z, U), n), z, lance_plane);
    
    auto bishops = pieces.to_ullong() & piece_id_set(BISHOP);
    for (int n: BitRange(bishops)) 
      for (auto dir: {UL, UR, DL, DR}) 
        fill_segment(state.pieceOf(n), state.pieceReach(dir, n), z, bishop_plane);
    
    auto rooks = pieces.to_ullong() & piece_id_set(ROOK);
    for (int n: BitRange(rooks)) 
      for (auto dir: {U, L, R, D}) 
        fill_segment(state.pieceOf(n), state.pieceReach(dir, n), z, rook_plane);

    for (auto dir: base8_directions()) 
      fill_segment(state.kingPiece(z), state.kingVisibilityBlackView(z, dir), z, king_plane);
  }

  std::ranges::copy(lance_plane,  ptr);
  std::ranges::copy(bishop_plane, ptr+2*81);
  std::ranges::copy(rook_plane,   ptr+4*81);
  std::ranges::copy(king_plane,   ptr+6*81);

  // checkmate, threatmate
  std::array<int,81*2> th_plane = {0};
  auto cmove = state.tryCheckmate1ply(), tmove = state.tryCheckmate1ply();
  auto fill_move = [&](Move move, std::array<int,81*2>& out) {
    if (! move.isNormal())
      return;
    auto offset = idx(move.player())*81;
    out[move.to().index81() + offset] = 1;
    auto from = move.from();
    if (! from.isPieceStand()) {
      if (move.oldPtype() == KNIGHT)
        th_plane[from.index81() + offset] = 1;
      else
        fill_segment(state.pieceAt(from), move.to(), move.player(), out);
    }
  };
  fill_move(cmove, th_plane);
  fill_move(tmove, th_plane);
  std::ranges::copy(th_plane, ptr+8*81);
}

py::array_t<float> pyosl::to_np_stdch(const EffectState& state) {
  auto feature = py::array_t<float>(9*9*(basic_channels+ext_channels));
  auto buffer = feature.request();
  auto ptr = static_cast<float_t*>(buffer.ptr);
  helper::write_np_44ch(state, ptr);
  helper::write_np_additional(state, ptr + 9*9*basic_channels);

  return feature;  
}
