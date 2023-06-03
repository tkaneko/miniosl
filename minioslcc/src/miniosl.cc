#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include "state.h"
#include "record.h"
#include "more.h"
#include <sstream>
#include <iostream>
#include <fstream>

namespace py = pybind11;

namespace mini {
  using namespace osl;
  Move to_move(const EffectState& state, std::string move);
  py::array_t<int8_t> to_np(const EffectState& state);
  py::array_t<int8_t> to_np_hand(const EffectState& state);
  py::array_t<int8_t> to_np_cover(const EffectState& state);
  py::array_t<uint64_t> to_np_pack(const EffectState& state);
  std::pair<MiniRecord, int> unpack_record(py::array_t<uint64_t> code_seq);
}


PYBIND11_MODULE(minioslcc, m) {
  m.doc() = "shogi utilities derived from osl";
  // classes
  typedef osl::EffectState state_t;
  py::class_<state_t>(m, "CCState", py::dynamic_attr(),
                                  "shogi state = board position + pieces in hand (mochigoma)")
    .def(py::init())
    .def(py::init<const state_t&>())
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
    .def("make_move", [](state_t &s, std::string input) {
      auto move = mini::to_move(s, input);
      if (! move.isNormal() || ! s.isLegal(move))
        throw std::runtime_error("move error "+input);
      s.makeMove(move);
    })
    .def("make_move", &state_t::makeMove)
    .def("to_usi", [](const state_t &s) { return osl::to_usi(s); })
    .def("to_csa", [](const state_t &s) { return osl::to_csa(s); })
    .def("__repr__", [](const state_t &s) {
      return "<CCState '" + osl::to_usi(s) + "'>";
    })
    .def("__str__", [](const state_t &s) { return osl::to_csa(s); })
    .def("turn", &state_t::turn, "player to move")
    .def("piece_at", &state_t::pieceAt, py::arg("square"), "a piece at given square")
    .def("count_cover", py::overload_cast<osl::Player,osl::Square>(&state_t::countEffect, py::const_),
         "the number of pieces reachable to given square")
    .def("pieces_cover", [](const state_t& s, osl::Player P, osl::Square target) {
      return (s.piecesOnBoard(P)&s.effectAt(target)).to_ullong();
    }, "the bitset of piece-ids reachable to given square")
    .def("count_hand", &state_t::countPiecesOnStand)
    .def("piece", &state_t::pieceOf, py::arg("id"))
    .def("in_check", py::overload_cast<>(&state_t::inCheck, py::const_))
    .def("in_checkmate", py::overload_cast<>(&state_t::inCheckmate, py::const_))
    .def("king_square", [](const state_t& s, osl::Player P) { return s.kingSquare(P); })
    .def("to_move", &mini::to_move, "parse and return move")
    .def("is_legal", &state_t::isLegal)
    .def("to_np", &mini::to_np, "pieces on board as numpy array")
    .def("to_np_hand", &mini::to_np_hand, "pieces on board as numpy array")
    .def("to_np_cover", &mini::to_np_cover, "pieces on board as numpy array")
    .def("to_np_pack", &mini::to_np_pack, "pack into 256bits")
    .def("encode_move", [](const state_t& s, osl::Move m) { return osl::bitpack::encode12(s, m); },
         "compress move into 12bits uint")
    .def("decode_move", [](const state_t& s, uint32_t c) { return osl::bitpack::decode_move12(s, c); },
         "uncompress move from 12bits uint")
  ;
  py::class_<osl::Square>(m, "Square", py::dynamic_attr())
    .def(py::init<int,int>())
    .def("x", &osl::Square::x)
    .def("y", &osl::Square::y)
    .def("to_xy", [](osl::Square sq) { return std::make_pair<int,int>(sq.x(), sq.y()); })
    .def("to_usi", [](osl::Square sq) { return osl::to_psn(sq); })
    .def("to_csa", [](osl::Square sq) { return osl::to_csa(sq); })
    .def("is_onboard", &osl::Square::isOnBoard)
    .def("__repr__", [](osl::Square sq) { return "<Square '"+osl::to_psn(sq) + "'>"; })
    .def("__str__", [](osl::Square sq) { return osl::to_csa(sq); })
    ;
  py::class_<osl::Move>(m, "Move", py::dynamic_attr())
    .def("src", &osl::Move::from)
    .def("dst", &osl::Move::to)
    .def("ptype", &osl::Move::ptype)
    .def("old_ptype", &osl::Move::oldPtype)
    .def("capture_ptype", &osl::Move::capturePtype)
    .def("is_promotion", &osl::Move::isPromotion)
    .def("is_drop", &osl::Move::isDrop)
    .def("is_normal", &osl::Move::isNormal)
    .def("to_usi", [](osl::Move m) { return osl::to_usi(m); })
    .def("to_csa", [](osl::Move m) { return osl::to_csa(m); })
    .def("__repr__", [](osl::Move m) { return "<Move '"+osl::to_psn(m) + "'>"; })
    .def("__str__", [](osl::Move m) { return osl::to_csa(m); })
    ;
  py::class_<osl::Piece>(m, "Piece", py::dynamic_attr())
    .def("square", &osl::Piece::square)
    .def("ptype", &osl::Piece::ptype)
    .def("color", &osl::Piece::owner)
    .def("is_piece", &osl::Piece::isPiece)
    .def("__repr__", [](osl::Piece p) {
      std::stringstream ss;
      ss << p;
      return "<Piece '"+ss.str()+ "'>";
    })
    ;
  py::class_<osl::MiniRecord>(m, "MiniRecord", py::dynamic_attr())
    .def_readonly("initial_state", &osl::MiniRecord::initial_state)
    .def_readonly("moves", &osl::MiniRecord::moves)
    .def_readonly("result", &osl::MiniRecord::result)
    .def_readonly("final_move", &osl::MiniRecord::final_move)
    .def("has_winner", &osl::MiniRecord::has_winner)
    .def("to_usi", py::overload_cast<const osl::MiniRecord&>(&osl::to_usi))
    .def("pack_record", [](const osl::MiniRecord& r){
      std::vector<uint64_t> code; osl::bitpack::append_binary_record(r, code);
      return code;
    }, "encode in uint64 array")
    .def("__len__", [](const osl::MiniRecord& r) { return r.moves.size(); })
    .def("__repr__", [](const osl::MiniRecord& r) {
      return "<MiniRecord '"+osl::to_usi(r.initial_state)
        + " " + std::to_string(r.moves.size()) + " moves'>"; })
    ;
  // functions
  m.def("csa_board", [](std::string input){
    try { return osl::csa::read_board(input); }
    catch (std::exception& e) { std::cerr << e.what() << '\n'; } return state_t();
  }, "parse and return State");
  m.def("usi_board", [](std::string input){
    auto record = osl::usi::read_record(input); return record.initial_state;
  }, "parse and return State");
  m.def("csa_record", py::overload_cast<std::string>(&osl::csa::read_record), "read str as a game record");
  m.def("csa_file", [](std::string filepath){
    return osl::csa::read_record(std::filesystem::path(filepath)); }, "load a game record");
  m.def("usi_record", &osl::usi::read_record, "read str as a game record");
  m.def("usi_file", [](std::string filepath, int id=0){
    std::ifstream is(filepath);
    std::string line;
    for (int i=0; i<id; ++i) getline(is, line);
    getline(is, line);
    return osl::usi::read_record(line); }, py::arg("arg"), py::arg("id")=0, "load a game record");
  m.def("unpack_record", &mini::unpack_record, "read record from np.array encoded by MiniRecord.pack_record");
  m.def("to_csa", py::overload_cast<osl::Ptype>(&osl::to_csa));
  m.def("to_csa", py::overload_cast<osl::Player>(&osl::to_csa));
  m.def("to_ja", py::overload_cast<osl::Square>(&osl::to_ki2));
  m.def("to_ja", py::overload_cast<osl::Ptype>(&osl::to_ki2));
  m.def("to_ja", py::overload_cast<osl::Move, const state_t&, osl::Square>(&osl::to_ki2),
        py::arg("move"), py::arg("state"), py::arg("prev_to")=osl::Square());
  
  // enums
  py::enum_<osl::Player>(m, "Player", py::arithmetic())
    .value("black", osl::BLACK).value("white", osl::WHITE)
    .export_values();
  py::enum_<osl::Ptype>(m, "Ptype", py::arithmetic(), "piece type")
    .value("pawn",   osl::PAWN).value("lance",  osl::LANCE).value("knight", osl::KNIGHT)
    .value("silver", osl::SILVER).value("gold",   osl::GOLD).value("bishop", osl::BISHOP)
    .value("rook",   osl::ROOK).value("king",   osl::KING)
    .value("ppawn",   osl::PPAWN, "promoted PAWN").value("plance",  osl::PLANCE, "promoted LANCE")
    .value("pknight", osl::PKNIGHT, "promoted KNIGHT").value("psilver", osl::PSILVER, "promoted SILVER")
    .value("pbishop", osl::PBISHOP, "promoted BISHOP").value("prook",   osl::PROOK, "promoted ROOK")
    .value("empty", osl::Ptype_EMPTY).value("edge",  osl::Ptype_EDGE)
    .export_values();
  py::enum_<osl::Direction>(m, "Direction", py::arithmetic())
    .value("UL", osl::UL).value("U",  osl::U).value("UR", osl::UR)
    .value("L",  osl::L).value("R",  osl::R)
    .value("DL", osl::DL).value("D",  osl::D).value("DR", osl::DR)
    .value("UUL", osl::UUL).value("UUR", osl::UUR)
    .value("Long_UL", osl::Long_UL).value("Long_U",  osl::Long_U).value("Long_UR", osl::Long_UR)
    .value("Long_L",  osl::Long_L).value("Long_R",  osl::Long_R)
    .value("Long_DL", osl::Long_DL).value("Long_D",  osl::Long_D).value("Long_DR", osl::Long_DR)
    .export_values();
  py::enum_<osl::GameResult>(m, "GameResult")
    .value("BlackWin", osl::BlackWin).value("WhiteWin", osl::WhiteWin)
    .value("Draw", osl::Draw).value("Interim", osl::Interim)
    .export_values();

  // data
  m.attr("ptype_move_direction") = &osl::ptype_move_direction;
  m.attr("ptype_piece_id") = &osl::ptype_piece_id;
  m.attr("ptype_csa_names") = &osl::ptype_csa_names;
  m.attr("ptype_en_names") = &osl::ptype_en_names;
  m.attr("piece_stand_order") = &osl::piece_stand_order;
  
  // "mapping of ptype to bitset of movable directions"

}

osl::Move mini::to_move(const EffectState& state, std::string move) {
  try {
    return usi::to_move(move, state);
  }
  catch (std::exception& e) {
  }
  try {
    return csa::to_move(move, state);
  }
  catch (std::exception& e) {
  }
  return Move::PASS(state.turn());
}

py::array_t<int8_t> mini::to_np(const EffectState& state) {
  auto feature = py::array_t<int8_t>(9*9);
  auto buffer = feature.request();
  auto ptr = static_cast<int8_t*>(buffer.ptr);
  for (int y: board_y_range())
    for (int x: board_x_range())
      ptr[(y-1)*9+(x-1)] = state.pieceAt(Square(x,y)).ptypeO();
  return feature;
}

py::array_t<int8_t> mini::to_np_hand(const EffectState& state) {
  const int N = piece_stand_order.size();
  auto feature = py::array_t<int8_t>(N*2);
  auto buffer = feature.request();
  auto ptr = static_cast<int8_t*>(buffer.ptr);
  for (auto pl: players)
    for (auto n: std::views::iota(0,N))
      ptr[n + 5*idx(pl)] = state.countPiecesOnStand(pl, piece_stand_order[n]);
  return feature;
}

py::array_t<int8_t> mini::to_np_cover(const EffectState& state) {
  auto feature = py::array_t<int8_t>(9*9*2);
  auto buffer = feature.request();
  auto ptr = static_cast<int8_t*>(buffer.ptr);
  for (auto pl: players)
    for (int y: board_y_range())
      for (int x: board_x_range())
        ptr[(y-1)*9+(x-1)+81*idx(pl)] = state.countEffect(pl, Square(x,y));
  return feature;
}

py::array_t<uint64_t> mini::to_np_pack(const EffectState& state) {
  auto packed = py::array_t<uint64_t>(4);
  auto buffer = packed.request();
  auto ptr = static_cast<uint64_t*>(buffer.ptr);
  osl::PackedState ps {state};
  auto bs = ps.to_bitset();
  for (int i: std::views::iota(0,4))
    ptr[i] = bs.binary[i];
  return packed;
}

std::pair<osl::MiniRecord, int> mini::unpack_record(py::array_t<uint64_t> code_seq) {
  auto buf = code_seq.request();
  auto ptr = static_cast<const uint64_t*>(buf.ptr);
  MiniRecord record;
  auto n = bitpack::read_binary_record(ptr, record);
  return std::make_pair(record, n);
}
