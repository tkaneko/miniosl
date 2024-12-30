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
#include <optional>

// namespace py = pybind11;

namespace pyosl {
  using namespace osl;
  Move read_japanese_move(const EffectState& state, std::u8string move, Square last_to=Square());
    
  /** squares covered by pieces as numpy array */
  py::array_t<int8_t> to_np_cover(const EffectState& state);

  /** a simple set of state features including board and hands */
  py::array_t<float> to_np_44ch(const BaseState& state);
  py::array_t<float> to_np_state_feature(const EffectState& state, bool flipped=false);

  std::tuple<py::array_t<int8_t>, int, int, py::array_t<int8_t>, py::array_t<uint8_t>>
  sample_np_feature_labels(const SubRecord& record, std::optional<int> idx=std::nullopt);
  void sample_np_feature_labels_to(const SubRecord& record, int offset,
                                   py::array_t<int8_t> inputs,
                                   py::array_t<int32_t> policy_labels,
                                   py::array_t<float> value_labels,
                                   py::array_t<int8_t> aux_labels,
                                   py::array_t<int8_t> inputs2,
                                   py::array_t<uint8_t> legalmove_label
                                   );
  void collate_features(const std::vector<std::vector<SubRecord>>& block_vector,
                        const py::list& indices,
                        py::array_t<int8_t> inputs,
                        py::array_t<int32_t> policy_labels,
                        py::array_t<float> value_labels,
                        py::array_t<int8_t> aux_labels,
                        std::optional<py::array_t<int8_t>> inputs2,
                        std::optional<py::array_t<uint8_t>> legalmove_labels,
                        std::optional<py::array_t<uint16_t>> sampled_id
                        );

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
  py::class_<base_t>(m, "BaseState", py::dynamic_attr(),
                     "parent of :py:class:`State`.   Please use `State` for usual cases.\n"
                     )
    .def("turn", &base_t::turn, "player to move")
    .def("oturn", [](const base_t& s){ return osl::alt(s.turn()); }, "opponent side to move")
    .def("piece_at", &base_t::pieceAt, "square"_a, "a piece at given square")
    .def("is_empty", [](const base_t& s, Square sq) { return s.pieceAt(sq).isEmpty(); },
         "square"_a, "true when no piece at square")
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
    .def("pawn_in_file",
         [](const base_t& state, osl::Player color, int x) { return state.pawnInFile(color, x); },
         "color"_a, "x"_a,
         "true if unpromoted pawn exists in file x")
    .def("to_np_44ch", &pyosl::to_np_44ch, "a simple set of state features including board and hands")
    .def("export_features", &pyosl::export_features, "moves"_a,
         "return np array of the standard set of features after moves are played")
    .def("export_features_after_move", &pyosl::export_features_after_move, "moves"_a, "lookahead"_a,
         "return pair of (1) np array of the standard set of features after moves and lookahead are played"
         " and (2) GameResult indicating game termination by the move")
    .def("decode_move_label", [](const base_t &s, int code) { return osl::ml::decode_move_label(code, s); },
         "code"_a,
         "interpret move index generated by :py:meth:`Move.policy_move_label`")
    .def("__repr__",
         [](const base_t &s) {
           return "<BaseState '" + osl::to_usi(s) + "'>";
         })
    .def("__str__", [](const base_t &s) { return osl::to_usi(s); })
    .def(py::self == py::self)
    .def(py::self != py::self)
    .def("__copy__",  [](const base_t& s) { return base_t(s);})
    .def("__deepcopy__",  [](const base_t& s, py::dict) { return base_t(s);}, "memo"_a)
    ;
  // state
  typedef osl::EffectState state_t;
  py::class_<state_t, base_t>(m, "State", py::dynamic_attr(),
                              "primary class of shogi state\n\n"
                              "functions: \n\n"
                              "- main: board position + pieces in hand (mochigoma)\n"
                              "- additional: covering pieces of each square, pins, etc\n\n"
                              ">>> state = miniosl.State()\n"
                              ">>> state.turn() == miniosl.black\n"
                              "True\n"
                              ">>> state.piece_at(miniosl.Square(5, 9)).ptype == miniosl.king\n"
                              "True\n"
                              ">>> state.king_square(miniosl.white) == miniosl.Square(5, 1)\n"
                              "True\n"
                              ">>> state.count_hand(miniosl.black, miniosl.pawn)\n"
                              "0\n"
                              )
    .def(py::init(), "make a initial state")
    .def(py::init<const state_t&>(), "src"_a, "make a copy of src")
    .def(py::init<const osl::BaseState&>(), "src"_a, "make a copy of src")
    .def("reset", &state_t::copyFrom, "src"_a, "re-initialize self copying `src`")
    .def("genmove",
         [](const state_t &s) {
           osl::MoveVector moves;
           s.generateLegal(moves);
           return moves;
         }, "genmove suitable for most cases (some moves are excluded)\n\n"
         ">>> s = miniosl.State()\n"
         ">>> len(s.genmove())\n"
         "30\n"
         )
    .def("genmove_full",
         [](const state_t &s) {
           osl::MoveVector moves;
           s.generateWithFullUnpromotions(moves);
           return moves;
         }, "genenerate full moves, including skip-promotion of pawn, bishop, and rook.")
    .def("genmove_check",
         [](const state_t &s) {
           osl::MoveVector moves;
           s.generateCheck(moves);
           return moves;
         }, "generate check moves only")
    .def("make_move", &state_t::make_move, "move_string"_a, "take usa or csa representation")
    .def("make_move",
         [](state_t &s, osl::Move move) {
           if (! s.isLegal(move) || ! s.isAcceptable(move))
             throw std::domain_error("move error "+osl::to_csa(move));
           s.makeMove(move);
         }, "move"_a)
    .def("make_move_pass", &state_t::makeMovePass)
    .def("__repr__",
         [](const state_t &s) {
           return "<State '" + osl::to_usi(s) + "'>";
         })
    .def("__str__", [](const state_t &s) { return osl::to_usi(s); })
    .def("rotate180", [](const state_t& s) { return state_t(s.rotate180()); })
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
    .def("pieces_cover",
         [](const state_t& s, osl::Player P, osl::Square target,
            std::optional<osl::Ptype> ptype, bool strict) {
           auto all = (s.piecesOnBoard(P)&s.effectAt(target)).to_ullong();
           if (ptype) {
             all &= piece_id_set(*ptype);
             if (strict) {
               auto promoted = s.promotedPieces().to_ullong();
               if (is_basic(*ptype))
                 all &= ~promoted;
               else
                 all &= promoted;
             }
           }
           return all;
         },
         "color"_a, "square"_a, "ptype"_a=std::nullopt, "strict_promotion"_a=false,
         "the bitset of piece-ids covering (reachable to) given square\n\n"
         ">>> s = miniosl.State()\n"
         ">>> pieces = s.pieces_cover(miniosl.black, miniosl.Square(5, 8))\n"
         ">>> bin(pieces).count('1')\n"
         "4\n"
         ">>> pieces = s.pieces_cover(miniosl.black, miniosl.Square(5, 8), miniosl.gold)\n"
         ">>> bin(pieces).count('1')\n"
         "2\n"
         )
    .def("pieces_on_board",
         [](const state_t& s, osl::Player P) { return s.piecesOnBoard(P).to_ullong(); },
         "color"_a, "the bitset of piece-ids on board (not captured) of color\n\n"
         ">>> s = miniosl.State()\n"
         ">>> bin(s.pieces_on_board(miniosl.black)).count('1')\n"
         "20\n"
         )
    .def("pieces_promoted",
         [](const state_t& s) { return s.promotedPieces().to_ullong(); },
         "the bitset of promoted piece-ids\n\n"
         ">>> s = miniosl.State()\n"
         ">>> s.pieces_promoted()\n"
         "0\n"
         )
    .def("long_piece_reach",
         [](const state_t& state, osl::Piece piece) {
           std::vector<std::pair<osl::Direction,osl::Square>> ret;
           auto color = piece.owner();
           if (piece.ptype() == osl::LANCE)
             ret.emplace_back(U, state.pieceReach(change_view(color, U), piece));
           else if (piece.ptype() == osl::BISHOP || piece.ptype() == osl::PBISHOP)
             for (auto d: {osl::UL, osl::UR, osl::DL, osl::DR})
               ret.emplace_back(d, state.pieceReach(change_view(color, d), piece));
           else if (piece.ptype() == osl::ROOK || piece.ptype() == osl::PROOK)
             for (auto d: {osl::U, osl::D, osl::L, osl::R})
               ret.emplace_back(d, state.pieceReach(change_view(color, d), piece));
           for (auto& e: ret)
             if (! e.second.isOnBoard())
               e.second -= to_offset(color, e.first);
           return ret;
         },
         "inspect furthest square reachable by long move of piece\n\n"
         ">>> s = miniosl.State()\n"
         ">>> s.long_piece_reach(s.piece_at(miniosl.Square(1, 9)))\n"
         "[(<Direction.U: 1>, <Square '1g'>)]\n"
         )
    .def("pinned",
         [](const state_t& s, osl::Player king) { return s.pin(king).to_ullong(); },
         "color"_a, "the bitset of pinned piece-ids of color")
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
    .def("__deepcopy__",  [](const state_t& s, py::dict) { return state_t(s);}, "memo"_a)
    .def("to_np_state_feature", &pyosl::to_np_state_feature, "flipped"_a=false, 
         "a subset of features without history information")
    ;

  py::class_<osl::SubRecord>(m, "SubRecord", py::dynamic_attr(),
                             "subset of MiniRecord, suitable when initial state is known")
    .def(py::init<const MiniRecord&>())
    .def_readonly("moves", &osl::SubRecord::moves, "list of :py:class:`Move` s")
    .def_readonly("result", &osl::SubRecord::result, ":py:class:`GameResult`")
    .def_readonly("final_move", &osl::SubRecord::final_move, "resign or win declaration in :py:class:`Move`")
    .def("sample_feature_labels", &pyosl::sample_np_feature_labels,
         "idx"_a=std::nullopt,
         "randomly samle index and call export_feature_labels()\n\n"
         ":param idx: move id or None for random,\n"
         ":returns: tuple of (input_features, move_label, value_label, aux_label, legal_moves).\n"
         )
    .def("sample_feature_labels_to", &pyosl::sample_np_feature_labels_to,
         "offset"_a, "inputs"_a, "policy_labels"_a, "value_labels"_a,
         "aux_labels"_a, "inputs2"_a, "legalmove_labels"_a,
         "randomly samle index and export features to given ndarray (must be zerofilled in advance)\n\n"
         ":param offset: offset in output index,\n"
         ":param inputs: input features to store,\n"
         ":param policy_labels: policy label to store,\n"
         ":param value_labels: value label to store,\n"
         ":param aux_labels: labels for auxiliary tasks to store,\n"
         ":param inputs2: input features for successor state to store,\n"
         ":param legalmove_labels: legal moves to store.\n"
         )
    .def("make_state", &osl::SubRecord::make_state, "n"_a, "make a state after the first `n` moves")
    ;
  
  // functions depends on np
  m.def("unpack_record", &pyosl::unpack_record, "read record from np.array encoded by MiniRecord.pack_record");
  m.def("collate_features",
        &pyosl::collate_features,
        "block_vector"_a, "indices"_a, "inputs"_a,
        "policy_labels"_a, "value_labels"_a, "aux_labels"_a,
        "inputs2"_a=std::nullopt, "legalmove_labels"_a=std::nullopt, "sampled_id"_a=std::nullopt,
        "collate function for `GameDataset`\n\n"
        ":param block_vector: game record db\n"
        ":param indices: list of pairs each of which forms (block_id, record_id)\n"
        ":param inputs: output buffer for all input features\n"
        ":param policy_labels: output buffer for all policy labels\n"
        ":param value_labels: output buffer for all value labels\n"
        ":param inputs2: afterstate features (optional)\n"
        ":param legalmoves: legal moves in bitset (optional)\n"
        ":param sampled_id: list of move_id sampled for each game (optional)\n"
        );
  m.def("parallel_threads", [](){ return osl::range_parallel_threads; },
        "internal concurrency");
  m.def("shogi816k",
        [](int id){ return osl::EffectState(osl::BaseState(osl::Shogi816K, id)); },
        "id"_a, "make a state");

  
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

std::tuple<py::array_t<int8_t>, int, int, py::array_t<int8_t>, py::array_t<uint8_t>>
pyosl::sample_np_feature_labels(const SubRecord& record, std::optional<int> idx) {
  nparray<int8_t> feature(ml::input_unit);
  nparray<int8_t> aux_feature(ml::aux_unit);
  nparray<uint8_t> legalmove_labels(ml::legalmove_bs_sz);

  std::fill(feature.ptr(), feature.ptr()+ml::input_unit, 0);
  std::fill(aux_feature.ptr(), aux_feature.ptr()+ml::aux_unit, 0);
  std::fill(legalmove_labels.ptr(),
            legalmove_labels.ptr() + ml::legalmove_bs_sz,
            0);
  
  int move_label, value_label;
  if (idx) {
    osl::MoveVector legal_moves;
    record.export_feature_labels(idx.value(),
                                 feature.ptr(), move_label, value_label,
                                 aux_feature.ptr(), legal_moves);
    ml::set_legalmove_bits(legal_moves, legalmove_labels.ptr());
  }
  else
    record.sample_feature_labels(feature.ptr(), move_label, value_label,
                                 aux_feature.ptr(), legalmove_labels.ptr());
  return {feature.array.reshape({-1, 9, 9}),
          move_label, value_label,
          aux_feature.array.reshape({-1, 9, 9}),
          legalmove_labels.array,
  };
}

void pyosl::sample_np_feature_labels_to(const SubRecord& record, int offset,
                                        py::array_t<int8_t> inputs,
                                        py::array_t<int32_t> policy_labels,
                                        py::array_t<float> value_labels,
                                        py::array_t<int8_t> aux_labels,
                                        py::array_t<int8_t> inputs2,
                                        py::array_t<uint8_t> legalmove_labels
                                        ) {
  auto input_buf = inputs.request(), policy_buf = policy_labels.request(),
    value_buf = value_labels.request(), aux_buf = aux_labels.request(),
    input2_buf = inputs2.request(),
    legalmove_buf = legalmove_labels.request();

  record.sample_feature_labels_to(offset,
                                  static_cast<nn_input_element*>(input_buf.ptr),
                                  static_cast<int32_t*>(policy_buf.ptr),
                                  static_cast<float*>(value_buf.ptr),
                                  static_cast<nn_input_element*>(aux_buf.ptr),
                                  static_cast<nn_input_element*>(input2_buf.ptr),
                                  static_cast<uint8_t*>(legalmove_buf.ptr),
                                  nullptr // sampled index
                                  );
}

void pyosl::collate_features(const std::vector<std::vector<SubRecord>>& block_vector,
                             const py::list& indices,
                             py::array_t<int8_t> inputs,
                             py::array_t<int32_t> policy_labels,
                             py::array_t<float> value_labels,
                             py::array_t<int8_t> aux_labels,
                             std::optional<py::array_t<int8_t>> inputs2_opt,
                             std::optional<py::array_t<uint8_t>> legalmove_labels_opt,
                             std::optional<py::array_t<uint16_t>> sampled_id_opt
                             )
{
  const int N = indices.size();
  auto inputs2 = inputs2_opt.value_or(py::array_t<int8_t>());
  auto legalmove_labels = legalmove_labels_opt.value_or(py::array_t<uint8_t>());
  auto sampled_id = sampled_id_opt.value_or(py::array_t<uint16_t>());
  
  auto input_buf = inputs.request(), policy_buf = policy_labels.request(),
    value_buf = value_labels.request(), aux_buf = aux_labels.request(),
    input2_buf = inputs2.request(),
    legalmove_buf = legalmove_labels.request(),
    sampledid_buf = sampled_id.request();
  auto *iptr = static_cast<nn_input_element*>(input_buf.ptr);
  auto *pptr = static_cast<int32_t*>(policy_buf.ptr);
  auto *vptr = static_cast<float*>(value_buf.ptr);
  auto *aptr = static_cast<nn_input_element*>(aux_buf.ptr);
  auto *i2ptr = inputs2_opt ? static_cast<nn_input_element*>(input2_buf.ptr) : nullptr;
  auto *lmptr = legalmove_labels_opt ? static_cast<uint8_t*>(legalmove_buf.ptr) : nullptr;
  auto *sidptr = sampled_id_opt ? static_cast<uint16_t*>(sampledid_buf.ptr) : nullptr;

  auto f = [&](int l, int r, TID tid) {
    for (int i=l; i<r; ++i) {
      py::tuple item = indices[i];
      int p = py::int_(item[0]), s = py::int_(item[1]);
      const SubRecord& record = block_vector[p][s];
      record.sample_feature_labels_to(i, iptr, pptr, vptr, aptr, i2ptr, lmptr, sidptr,
                                      SubRecord::default_decay, tid);
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
