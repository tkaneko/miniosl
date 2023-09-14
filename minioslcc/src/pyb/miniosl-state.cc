#include "pyb/miniosl.h"
#include "pyb/np.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/operators.h>
#include "state.h"
#include "record.h"
#include "feature.h"
#include "game.h"
#include "impl/bitpack.h"
#include "impl/more.h"
#include "impl/checkmate.h"
#include "impl/range-parallel.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <thread>

// namespace py = pybind11;

namespace pyosl {
  using namespace osl;
  Move read_japanese_move(const EffectState& state, std::u8string move, Square last_to=Square());
    
  /** squares covered by pieces as numpy array */
  py::array_t<int8_t> to_np_cover(const EffectState& state);

  /** a simple set of state features including board and hands */
  py::array_t<float> to_np_44ch(const BaseState& state);
  py::array_t<float> to_np_state_feature(const EffectState& state, bool flipped=false);
  std::tuple<py::array_t<float>, int, int, py::array_t<float>>
  to_np_feature_labels(const StateRecord320& record);

  std::tuple<py::array_t<int8_t>, int, int, py::array_t<int8_t>>
  sample_np_feature_labels(const SubRecord& record);
  void sample_np_feature_labels_to(const SubRecord& record, int offset,
                                   py::array_t<int8_t> inputs,
                                   py::array_t<int32_t> policy_labels,
                                   py::array_t<float> value_labels,
                                   py::array_t<int8_t> aux_labels);
  void collate_features(const std::vector<std::vector<SubRecord>>& block_vector,
                        const py::list& indices,
                        py::array_t<int8_t> inputs,
                        py::array_t<int32_t> policy_labels,
                        py::array_t<float> value_labels,
                        py::array_t<int8_t> aux_labels);

  /** pack into 256bits */
  py::array_t<uint64_t> to_np_pack(const BaseState& state);
  std::pair<MiniRecord, int> unpack_record(py::array_t<uint64_t> code_seq);

  py::array_t<float> export_features(BaseState initial, const MoveVector& moves);
  std::pair<py::array_t<float>,osl::GameResult> export_features_after_move(BaseState initial, const MoveVector& moves, Move);
}


void pyosl::init_state_np(py::module_& m) {
  using namespace pybind11::literals;

  // define the base class prior to the main state class
  typedef osl::BaseState base_t;
  py::class_<base_t>(m, "BaseState", py::dynamic_attr(), "parent of :py:class:`State`.   Please use `State` for usual cases.\n\n"
                     ">>> state = miniosl.State()\n"
                     ">>> state.turn() == miniosl.black\n"
                     "True\n"
                     ">>> state.piece_at(miniosl.Square(5, 9)).ptype() == miniosl.king\n"
                     "True\n"
                     ">>> state.king_square(miniosl.white) == miniosl.Square(5, 1)\n"
                     "True\n"
                     ">>> state.count_hand(miniosl.black, miniosl.pawn)\n"
                     "0\n"
                     )
    .def("turn", &base_t::turn, "player to move")
    .def("piece_at", &base_t::pieceAt, "square"_a, "a piece at given square")
    .def("piece", &base_t::pieceOf, "internal_id"_a)
    .def("count_hand", &base_t::countPiecesOnStand, "color"_a, "ptype"_a)
    .def("king_square", [](const base_t& s, osl::Player P) { return s.kingSquare(P); }, "color"_a)
    .def("to_usi", [](const base_t &s) { return osl::to_usi(s); })
    .def("to_csa", [](const base_t &s) { return osl::to_csa(s); })
    .def("rotate180", &base_t::rotate180,
         "make a rotated state\n\n"
         ">>> s = miniosl.State()\n"
         ">>> print(s.to_csa(), end='')\n"
         "P1-KY-KE-GI-KI-OU-KI-GI-KE-KY\n"
         "P2 * -HI *  *  *  *  * -KA * \n"
         "P3-FU-FU-FU-FU-FU-FU-FU-FU-FU\n"
         "P4 *  *  *  *  *  *  *  *  * \n"
         "P5 *  *  *  *  *  *  *  *  * \n"
         "P6 *  *  *  *  *  *  *  *  * \n"
         "P7+FU+FU+FU+FU+FU+FU+FU+FU+FU\n"
         "P8 * +KA *  *  *  *  * +HI * \n"
         "P9+KY+KE+GI+KI+OU+KI+GI+KE+KY\n"
         "+\n"
         ">>> _ = s.make_move('+7776FU')\n"
         ">>> print(s.rotate180().to_csa(), end='')\n"
         "P1-KY-KE-GI-KI-OU-KI-GI-KE-KY\n"
         "P2 * -HI *  *  *  *  * -KA * \n"
         "P3-FU-FU-FU-FU-FU-FU * -FU-FU\n"
         "P4 *  *  *  *  *  * -FU *  * \n"
         "P5 *  *  *  *  *  *  *  *  * \n"
         "P6 *  *  *  *  *  *  *  *  * \n"
         "P7+FU+FU+FU+FU+FU+FU+FU+FU+FU\n"
         "P8 * +KA *  *  *  *  * +HI * \n"
         "P9+KY+KE+GI+KI+OU+KI+GI+KE+KY\n"
         "+\n")
    .def("hash_code", &osl::hash_code, "64bit int for board and 32bit for (black) hand pieces")
    .def("to_np_44ch", &pyosl::to_np_44ch, "a simple set of state features including board and hands")
    .def("export_features", &pyosl::export_features, "moves"_a,
         "return np array of the standard set of features after moves are played")
    .def("export_features_after_move", &pyosl::export_features_after_move, "moves"_a, "lookahead"_a,
         "return pair of (1) np array of the standard set of features after moves and lookahead are played"
         " and (2) GameResult indicating game termination by the move")
    .def("to_np_pack", &pyosl::to_np_pack, "pack into 256bits")
    .def("decode_move_label", [](const base_t &s, int code) { return osl::ml::decode_move_label(code, s); },
         "code"_a,
         "interpret move index generated by :py:meth:`Move.policy_move_label`")
    .def("__repr__", [](const base_t &s) {
      return "<BaseState '" + osl::to_usi(s) + "'>";
    })
    .def("__str__", [](const base_t &s) { return osl::to_usi(s); })
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
    .def("reset", &state_t::copyFrom, "src"_a, "re-initialize self copying `src`")
    .def("genmove", [](const state_t &s) {
      osl::MoveVector moves;
      s.generateLegal(moves);
      return moves;
    }, "genmove suitable for most cases (some moves are excluded)\n\n"
      ">>> s = miniosl.State()\n"
      ">>> len(s.genmove())\n"
      "30\n"
      )
    .def("genmove_full", [](const state_t &s) {
      osl::MoveVector moves;
      s.generateWithFullUnpromotions(moves);
      return moves;
    }, "genenerate full moves, including skip-promotion of pawn, bishop, and rook.")
    .def("genmove_check", [](const state_t &s) {
      osl::MoveVector moves;
      s.generateCheck(moves);
      return moves;
    }, "generate check moves only")
    .def("make_move", &state_t::make_move, "move_string"_a, "take usa or csa representation")
    .def("make_move", [](state_t &s, osl::Move move) {
      if (! s.isLegal(move) || ! s.isAcceptable(move))
        throw std::domain_error("move error "+osl::to_csa(move));
      s.makeMove(move);
    }, "move"_a)
    .def("make_move_pass", &state_t::makeMovePass)
    .def("__repr__", [](const state_t &s) {
      return "<State '" + osl::to_usi(s) + "'>";
    })
    .def("__str__", [](const state_t &s) { return osl::to_usi(s); })
    .def("count_cover", py::overload_cast<osl::Player,osl::Square>(&state_t::countEffect, py::const_),
         "color"_a, "sq"_a,
         "the number of pieces reachable to given square\n\n"
         ">>> s = miniosl.State()\n"
         ">>> s.count_cover(miniosl.black, miniosl.Square(1, 7))\n"
         "2\n"
         ">>> s.count_cover(miniosl.black, miniosl.Square(5, 7))\n"
         "0\n"
         ">>> s.count_cover(miniosl.black, miniosl.Square(9, 7))\n"
         "3\n"
         )
    .def("pieces_cover", [](const state_t& s, osl::Player P, osl::Square target) {
      return (s.piecesOnBoard(P)&s.effectAt(target)).to_ullong();
    }, "color"_a, "square"_a, "the bitset of piece-ids reachable to given square")
    .def("in_check", py::overload_cast<>(&state_t::inCheck, py::const_))
    .def("in_checkmate", &state_t::inCheckmate)
    .def("in_no_legalmoves", &state_t::inNoLegalMoves)
    .def("win_if_declare", &win_if_declare)
    .def("to_move", &state_t::to_move, "move_string"_a, "parse and return move\n\n"
         ">>> s = miniosl.State()\n"
         ">>> s.to_move('+2726FU') == s.to_move('2g2f')\n"
         "True\n"
         )
    .def("read_japanese_move", &pyosl::read_japanese_move,
         "move"_a, "last_to"_a=Square(),
         "parse and return move")
    .def("is_legal", &state_t::isLegal, "move"_a)
    .def("to_np_cover", &pyosl::to_np_cover, "squares covered by pieces as numpy array")
    .def("encode_move", [](const state_t& s, osl::Move m) { return osl::bitpack::encode12(s, m); },
         "move"_a,
         ":meta private: compress move into 12bits uint")
    .def("decode_move", [](const state_t& s, uint32_t c) { return osl::bitpack::decode_move12(s, c); },
         "code"_a,
         ":meta private: uncompress move from 12bits uint generated by :py:meth:`encode_move`")
    .def("try_checkmate_1ply", &state_t::tryCheckmate1ply, "try to find a checkmate move")
    .def("__copy__",  [](const state_t& s) { return state_t(s);})
    .def("__deepcopy__",  [](const state_t& s) { return state_t(s);})
    .def("to_np_state_feature", &pyosl::to_np_state_feature, "flipped"_a=false, 
         "a subset of features without history information")
    ;

  py::class_<osl::StateRecord256>(m, "StateRecord256", py::dynamic_attr(),
                                  "training record as a (state, move, and result) tuple in 32 bytes")
    .def_readonly("state", &osl::StateRecord256::state, ":py:class:`BaseState`, input in training")
    .def_readonly("move", &osl::StateRecord256::next, ":py:class:`Move` to play, label in training")
    .def_readonly("result", &osl::StateRecord256::result, ":py:class:`GameResult`, label in training")
    .def_readonly("flipped", &osl::StateRecord256::flipped, "bool, true when the original state was white to move")
    .def("to_bitset", &osl::StateRecord256::to_bitset)
    .def("restore", &osl::StateRecord256::restore, "code"_a)
    .def("to_np_44ch", [](const osl::StateRecord256& obj) {
      return pyosl::to_np_44ch(obj.state);
    })
    .def("to_np_state_feature", [](const osl::StateRecord256& obj) {
      osl::EffectState state(obj.state);
      return pyosl::to_np_state_feature(state, obj.flipped);
    }, "a subset set of features without history")
    .def("__copy__",  [](const osl::StateRecord256& r) { return osl::StateRecord256(r);})
    .def("__deepcopy__",  [](const osl::StateRecord256& r) { return osl::StateRecord256(r);})
    ;  
  py::class_<osl::StateRecord320>(m, "StateRecord320", py::dynamic_attr(),
                                  "training record as a (state-with-5-len-history, move, and result) tuple in 40 bytes")
    .def_readonly("base", &osl::StateRecord320::base, ":py:class:`BaseState`")
    .def_readonly("history", &osl::StateRecord320::history, "last five :py:class:`Move` s, additional input in training")
    .def("to_bitset", &osl::StateRecord320::to_bitset)
    .def("restore", &osl::StateRecord320::restore, "code"_a)
    .def("make_state", &osl::StateRecord320::make_state, "target state to learn")
    .def("last_move", &osl::StateRecord320::last_move, "last :py:class:`Move` played")
    .def("to_np_state_feature", [](const osl::StateRecord320& obj) {
      osl::EffectState state(obj.make_state());
      return pyosl::to_np_state_feature(state, obj.base.flipped);
    }, "a subset of features without history")
    .def("to_np_feature_labels", &pyosl::to_np_feature_labels,
         "tuple of standard set of features, move label, value label, and others")
    .def("__copy__",  [](const osl::StateRecord320& r) { return osl::StateRecord320(r);})
    .def("__deepcopy__",  [](const osl::StateRecord320& r) { return osl::StateRecord320(r);})
    .def_static("test_item", [](){ osl::StateRecord320 r; r.base.next = Move(Square(7,7),Square(7,6),PAWN,Ptype_EMPTY,false,BLACK); return r; })  // for test
    ;  
  py::class_<osl::SubRecord>(m, "SubRecord", py::dynamic_attr(), "subset of MiniRecord")
    .def(py::init<const MiniRecord&>())
    .def_readonly("moves", &osl::SubRecord::moves, "list of :py:class:`Move` s")
    .def_readonly("result", &osl::SubRecord::result, ":py:class:`GameResult`")
    .def_readonly("final_move", &osl::SubRecord::final_move, "resign or win declaration in :py:class:`Move`")
    .def("sample_feature_labels", &pyosl::sample_np_feature_labels, "randomly samle index and call export_feature_labels()")
    .def("sample_feature_labels_to", &pyosl::sample_np_feature_labels_to, "randomly samle index and export features to given ndarray (must be zerofilled in advance)")
    .def("make_state", &osl::SubRecord::make_state, "n"_a, "make a state after the first `n` moves")
    ;
  
  // functions depends on np
  m.def("unpack_record", &pyosl::unpack_record, "read record from np.array encoded by MiniRecord.pack_record");
  m.def("to_state_label_tuple256", [](std::array<uint64_t,4> binary){
    osl::StateRecord256 obj;
    obj.restore(binary);
    return obj;
  }, "code"_a, "unpack four uint64s to get StateRecord256");
  m.def("to_state_label_tuple320", [](std::array<uint64_t,5> binary){
    osl::StateRecord320 obj;
    obj.restore(binary);
    return obj;
  }, "code"_a, "unpack five uint64s to get StateRecord320");
  m.def("collate_features", &pyosl::collate_features, "collate function for `GameDataset`");
  m.def("parallel_threads", [](){ return osl::range_parallel_threads; },
        "internal concurrency");
  
  py::bind_vector<std::vector<osl::SubRecord>>(m, "GameRecordBlock");
  py::bind_vector<std::vector<std::vector<osl::SubRecord>>>(m, "GameBlockVector")
    .def("reserve",  &std::vector<std::vector<osl::SubRecord>>::reserve, "reserves storage");;
}

osl::Move pyosl::read_japanese_move(const EffectState& state, std::u8string move, Square last_to) {
  try {
    return kanji::to_move(move, state, last_to);
  }
  catch (std::exception& e) {
  }
  return Move::PASS(state.turn());
}

py::array_t<int8_t> pyosl::to_np_cover(const EffectState& state) {
  nparray<int8_t> feature(9*9*2);
  auto ptr = feature.ptr();
  for (auto pl: players)
    for (int y: board_y_range())
      for (int x: board_x_range())
        ptr[(y-1)*9+(x-1)+81*idx(pl)] = state.countEffect(pl, Square(x,y));
  return feature.array.reshape({-1, 9, 9});
}

py::array_t<uint64_t> pyosl::to_np_pack(const BaseState& state) {
  nparray<uint64_t> packed(4);
  auto ptr = packed.ptr();
  osl::StateRecord256 instance {state};
  auto bs = instance.to_bitset();
  for (int i: std::views::iota(0,4))
    ptr[i] = bs[i];
  return packed.array;
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
  auto sz = 9*9*ml::basic_channels;
  nparray<float> feature(sz);
  ml::write_float_feature([&](auto *out){ ml::helper::write_np_44ch(state, out); },
                          sz,
                          feature.ptr()
                          );
  return feature.array;
}

py::array_t<float> pyosl::to_np_state_feature(const EffectState& state, bool flipped) {
  auto sz = 9*9*ml::board_channels;
  nparray<float> feature(sz);

  ml::write_float_feature([&](auto *out){ ml::helper::write_state_features(state, flipped, out); },
                          sz,
                          feature.ptr());
  
  return feature.array.reshape({-1, 9, 9});
}

std::tuple<py::array_t<float>, int, int, py::array_t<float>>
pyosl::to_np_feature_labels(const StateRecord320& record) {
  nparray<float> feature(ml::input_unit);
  nparray<float> aux_feature(ml::aux_unit);

  int move_label, value_label;
  record.export_feature_labels(feature.ptr(), move_label, value_label, aux_feature.ptr());
  return {feature.array.reshape({-1, 9, 9}), move_label, value_label, aux_feature.array.reshape({-1, 9, 9})};
}

std::tuple<py::array_t<int8_t>, int, int, py::array_t<int8_t>>
pyosl::sample_np_feature_labels(const SubRecord& record) {
  nparray<int8_t> feature(ml::input_unit);
  nparray<int8_t> aux_feature(ml::aux_unit);

  std::fill(feature.ptr(), feature.ptr()+ml::input_unit, 0);
  std::fill(aux_feature.ptr(), aux_feature.ptr()+ml::aux_unit, 0);
  
  int move_label, value_label;
  record.sample_feature_labels(feature.ptr(), move_label, value_label, aux_feature.ptr());
  return {feature.array.reshape({-1, 9, 9}), move_label, value_label, aux_feature.array.reshape({-1, 9, 9})};
}

void pyosl::sample_np_feature_labels_to(const SubRecord& record, int offset,
                                        py::array_t<int8_t> inputs,
                                        py::array_t<int32_t> policy_labels,
                                        py::array_t<float> value_labels,
                                        py::array_t<int8_t> aux_labels) {
  auto input_buf = inputs.request(), policy_buf = policy_labels.request(),
    value_buf = value_labels.request(), aux_buf = aux_labels.request();

  record.sample_feature_labels_to(offset,
                                  static_cast<nn_input_element*>(input_buf.ptr),
                                  static_cast<int32_t*>(policy_buf.ptr),
                                  static_cast<float*>(value_buf.ptr),
                                  static_cast<nn_input_element*>(aux_buf.ptr));
}

void pyosl::collate_features(const std::vector<std::vector<SubRecord>>& block_vector,
                             const py::list& indices,
                             py::array_t<int8_t> inputs,
                             py::array_t<int32_t> policy_labels,
                             py::array_t<float> value_labels,
                             py::array_t<int8_t> aux_labels)
{
  const int N = indices.size();
  auto input_buf = inputs.request(), policy_buf = policy_labels.request(),
    value_buf = value_labels.request(), aux_buf = aux_labels.request();
  auto *iptr = static_cast<nn_input_element*>(input_buf.ptr);
  auto *pptr = static_cast<int32_t*>(policy_buf.ptr);
  auto *vptr = static_cast<float*>(value_buf.ptr);
  auto *aptr = static_cast<nn_input_element*>(aux_buf.ptr);

  auto f = [&](int l, int r, TID tid) {
    for (int i=l; i<r; ++i) {
      py::tuple item = indices[i];
      int p = py::int_(item[0]), s = py::int_(item[1]);
      const SubRecord& record = block_vector[p][s];
      record.sample_feature_labels_to(i, iptr, pptr, vptr, aptr, SubRecord::default_decay, tid);
    }
  };
  run_range_parallel_tid(N, f);
}

py::array_t<float> pyosl::export_features(BaseState initial, const MoveVector& moves) {
  nparray<float> feature(ml::input_unit);
  ml::write_float_feature([&](auto *out){ ml::export_features(initial, moves, out); },
                          ml::input_unit,
                          feature.ptr());
  return feature.array.reshape({-1, 9, 9});
}

std::pair<py::array_t<float>,osl::GameResult>
pyosl::export_features_after_move(BaseState initial, const MoveVector& moves, Move latest) {
  nparray<float> feature(ml::input_unit);
  std::fill(feature.ptr(), feature.ptr()+ml::input_unit, 0);

  auto ret =
    ml::write_float_feature([&](auto *out){
      return GameManager::export_heuristic_feature_after(latest, initial, moves, out);
    },
      ml::input_unit,
      feature.ptr());
  return {feature.array.reshape({-1, 9, 9}), ret};
}
