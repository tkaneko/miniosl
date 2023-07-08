#include "pyb/miniosl.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/operators.h>
#include "state.h"
#include "record.h"
#include "feature.h"
#include "game.h"
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

  const int basic_channels = 44;
  /** a simple set of state features including board and hands */
  py::array_t<float> to_np_44ch(const BaseState& state);
  /** batch version of to_np_44ch */
  std::pair<py::array_t<float>, py::array_t<int8_t>>
  to_np_batch_44ch(const std::vector<std::array<uint64_t,4>>& batch);
  py::array_t<float> to_np_heuristic(const EffectState& state, bool flipped=false, Move last_move=Move());
  std::tuple<py::array_t<float>, int, int, py::array_t<float>>
  to_np_feature_labels(const StateRecord320& record);

  /** pack into 256bits */
  py::array_t<uint64_t> to_np_pack(const BaseState& state);
  std::pair<MiniRecord, int> unpack_record(py::array_t<uint64_t> code_seq);

  py::array_t<float> export_heuristic_feature(const GameManager& mgr);
  py::array_t<float> export_heuristic_feature_parallel(const ParallelGameManager& mgr);

  py::array_t<float> export_heuristic_feature_static(const EffectState&, Move=Move());
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
    .def("rotate180", &base_t::rotate180)
    .def("hash_code", &osl::hash_code, "64bit int for board and 32bit for (black) hand pieces")
    .def("to_np", &pyosl::to_np, "pieces on board as numpy array")
    .def("to_np_hand", &pyosl::to_np_hand, "pieces on hand as numpy array")
    .def("to_np_44ch", &pyosl::to_np_44ch, "a simple set of state features including board and hands")
    .def("to_np_pack", &pyosl::to_np_pack, "pack into 256bits")
    .def("decode_move_label", [](const base_t &s, int code) { return osl::ml::decode_move_label(code, s); })
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
    .def("to_np_heuristic", &pyosl::to_np_heuristic, py::arg("flipped")=false, py::arg("last_move")=osl::Move(), 
         "standard set of features")
    ;

  py::class_<osl::StateRecord256>(m, "StateRecord256", py::dynamic_attr())
    .def_readonly("state", &osl::StateRecord256::state)
    .def_readonly("move", &osl::StateRecord256::next)
    .def_readonly("result", &osl::StateRecord256::result)
    .def_readonly("flipped", &osl::StateRecord256::flipped)
    .def("to_bitset", &osl::StateRecord256::to_bitset)
    .def("restore", &osl::StateRecord256::restore)
    .def("to_np_44ch", [](const osl::StateRecord256& obj) {
      return pyosl::to_np_44ch(obj.state);
    })
    .def("to_np_heuristic", [](const osl::StateRecord256& obj) {
      osl::EffectState state(obj.state);
      return pyosl::to_np_heuristic(state, obj.flipped);
    })
    .def("__copy__",  [](const osl::StateRecord256& r) { return osl::StateRecord256(r);})
    .def("__deepcopy__",  [](const osl::StateRecord256& r) { return osl::StateRecord256(r);})
    ;  
  py::class_<osl::StateRecord320>(m, "StateRecord320", py::dynamic_attr())
    .def_readonly("base", &osl::StateRecord320::base)
    .def_readonly("history", &osl::StateRecord320::history)
    .def("to_bitset", &osl::StateRecord320::to_bitset)
    .def("restore", &osl::StateRecord320::restore)
    .def("make_state", &osl::StateRecord320::make_state)
    .def("last_move", &osl::StateRecord320::last_move)
    .def("to_np_heuristic", [](const osl::StateRecord320& obj) {
      osl::EffectState state(obj.make_state());
      return pyosl::to_np_heuristic(state, obj.base.flipped, obj.last_move());
    })
    .def("to_np_feature_labels", &pyosl::to_np_feature_labels)
    .def("__copy__",  [](const osl::StateRecord320& r) { return osl::StateRecord320(r);})
    .def("__deepcopy__",  [](const osl::StateRecord320& r) { return osl::StateRecord320(r);})
    ;  
  
  py::class_<osl::GameManager>(m, "GameManager", py::dynamic_attr())
    .def(py::init<>())
    .def_readonly("record", &osl::GameManager::record)
    .def_readonly("state", &osl::GameManager::state)
    .def("add_move", &osl::GameManager::add_move)
    .def("export_heuristic_feature", &pyosl::export_heuristic_feature)
    ;
  py::class_<osl::ParallelGameManager>(m, "ParallelGameManager", py::dynamic_attr())
    .def(py::init<int,bool>())
    .def_readonly("games", &osl::ParallelGameManager::games)
    .def_readonly("completed_games", &osl::ParallelGameManager::completed_games)
    .def("add_move_parallel", &osl::ParallelGameManager::add_move_parallel)
    .def("export_heuristic_feature_parallel", &pyosl::export_heuristic_feature_parallel)
    .def("n_parallel", &ParallelGameManager::n_parallel)
    ;

  // functions depends on np
  m.def("unpack_record", &pyosl::unpack_record, "read record from np.array encoded by MiniRecord.pack_record");
  m.def("to_np_batch_44ch", &pyosl::to_np_batch_44ch, "batch conversion of to_np_44ch");
  m.def("to_state_label_tuple", [](std::array<uint64_t,4> binary){
    osl::StateRecord256 obj;
    obj.restore(binary);
    return obj;
  });
  m.def("to_state_label_tuple320", [](std::array<uint64_t,5> binary){
    osl::StateRecord320 obj;
    obj.restore(binary);
    return obj;
  });
  m.def("export_heuristic_feature", &pyosl::export_heuristic_feature_static,
        py::arg("state"), py::arg("last_move")=osl::Move());
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
  auto board = ml::board_dense_feature(state);
  std::ranges::copy(board, ptr);
  return feature;
}

py::array_t<int8_t> pyosl::to_np_hand(const BaseState& state) {
  const int N = piece_stand_order.size();
  auto feature = py::array_t<int8_t>(N*2);
  auto buffer = feature.request();
  auto ptr = static_cast<int8_t*>(buffer.ptr);
  auto hand = ml::hand_dense_feature(state);
  std::ranges::copy(hand, ptr);
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
  auto feature = py::array_t<float>(9*9*basic_channels);
  auto buffer = feature.request();
  auto ptr = static_cast<float_t*>(buffer.ptr);
  ml::helper::write_np_44ch(state, ptr);
  return feature;
}

std::pair<py::array_t<float>, py::array_t<int8_t>>
pyosl::to_np_batch_44ch(const std::vector<std::array<uint64_t,4>>& batch) {
  auto batch_feature = py::array_t<float_t>(batch.size()*9*9*basic_channels);
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
      ml::helper::write_np_44ch(obj.state, &ptr[i*9*9*basic_channels]);
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

py::array_t<float> pyosl::to_np_heuristic(const EffectState& state, bool flipped, Move last_move) {
  auto feature = py::array_t<float>(9*9*ml::channel_id.size());
  auto buffer = feature.request();
  auto ptr = static_cast<float_t*>(buffer.ptr);

  ml::helper::write_np_44ch(state, ptr);
  ml::helper::write_np_additional(state, flipped, last_move, ptr + 9*9*basic_channels);

  return feature;  
}

std::tuple<py::array_t<float>, int, int, py::array_t<float>>
pyosl::to_np_feature_labels(const StateRecord320& record) {
  auto aux_feature = py::array_t<float>(9*9*12);
  auto aux_buffer = aux_feature.request();
  auto aux_ptr = static_cast<float_t*>(aux_buffer.ptr);

  osl::EffectState state(record.make_state());
  ml::helper::write_np_aftermove(state, record.base.next, aux_ptr);

  return {pyosl::to_np_heuristic(state, record.base.flipped, record.last_move()),
          ml::policy_move_label(record.base.next), ml::value_label(record.base.result), aux_feature};
}

py::array_t<float> pyosl::export_heuristic_feature(const osl::GameManager& mgr) {
  auto feature = py::array_t<float>(9*9*ml::channel_id.size());
  auto buffer = feature.request();
  auto ptr = static_cast<float_t*>(buffer.ptr);
  mgr.export_heuristic_feature(ptr);
  return feature;
}

py::array_t<float> pyosl::export_heuristic_feature_parallel(const osl::ParallelGameManager& mgrs) {
  auto feature = py::array_t<float>(9*9*ml::channel_id.size()*mgrs.n_parallel());
  auto buffer = feature.request();
  auto ptr = static_cast<float_t*>(buffer.ptr);
  mgrs.export_heuristic_feature_parallel(ptr);
  return feature;
}

py::array_t<float> pyosl::export_heuristic_feature_static(const EffectState& state, Move last_move) {
  auto feature = py::array_t<float>(9*9*ml::channel_id.size());
  auto buffer = feature.request();
  auto ptr = static_cast<float_t*>(buffer.ptr);
  GameManager::export_heuristic_feature(state, last_move, ptr);
  return feature;
}

