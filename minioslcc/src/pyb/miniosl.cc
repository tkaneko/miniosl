#include "pyb/miniosl.h"
#include <pybind11/stl.h>
#include <pybind11/operators.h>

#include "state.h"
#include "record.h"
#include "impl/bitpack.h"
#include "impl/more.h"
#include <sstream>
#include <iostream>
#include <fstream>

PYBIND11_MODULE(minioslcc, m) {
  m.doc() = "shogi utilities derived from osl";
  pyosl::init_basic(m);
  pyosl::init_state_np(m);  
}

void pyosl::init_basic(py::module_& m) {
  m.doc() = "shogi utilities derived from osl";
  // classes
  py::class_<osl::Square>(m, "Square", py::dynamic_attr())
    .def(py::init<>())
    .def(py::init<int,int>())
    .def("x", &osl::Square::x)
    .def("y", &osl::Square::y)
    .def("to_xy", [](osl::Square sq) { return std::make_pair<int,int>(sq.x(), sq.y()); })
    .def("to_usi", [](osl::Square sq) { return osl::to_psn(sq); })
    .def("to_csa", [](osl::Square sq) { return osl::to_csa(sq); })
    .def("is_onboard", &osl::Square::isOnBoard)
    .def("is_piece_stand", &osl::Square::isPieceStand)
    .def("index81", py::overload_cast<>(&osl::Square::index81, py::const_))
    .def("__repr__", [](osl::Square sq) { return "<Square '"+osl::to_psn(sq) + "'>"; })
    .def("__str__", [](osl::Square sq) { return osl::to_csa(sq); })
    .def(py::self == py::self)
    .def(py::self != py::self)
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
    .def("is_capture", &osl::Move::isCapture)
    .def("color", &osl::Move::player)
    .def("to_usi", [](osl::Move m) { return osl::to_usi(m); })
    .def("to_csa", [](osl::Move m) { return osl::to_csa(m); })
    .def("__repr__", [](osl::Move m) { return "<Move '"+osl::to_psn(m) + "'>"; })
    .def("__str__", [](osl::Move m) { return osl::to_csa(m); })
    .def(py::self == py::self)
    .def(py::self != py::self)
    ;
  py::class_<osl::Piece>(m, "Piece", py::dynamic_attr())
    .def("square", &osl::Piece::square)
    .def("ptype", &osl::Piece::ptype)
    .def("color", &osl::Piece::owner)
    .def("is_piece", &osl::Piece::isPiece)
    .def("id", &osl::Piece::id)
    .def("__repr__", [](osl::Piece p) {
      std::stringstream ss;
      ss << p;
      return "<Piece '"+ss.str()+ "'>";
    })
    .def(py::self == py::self)
    .def(py::self != py::self)
    ;
  
  py::class_<osl::MiniRecord>(m, "MiniRecord", py::dynamic_attr())
    .def(py::init<>())
    .def_readonly("initial_state", &osl::MiniRecord::initial_state)
    .def_readonly("moves", &osl::MiniRecord::moves)
    .def_readonly("result", &osl::MiniRecord::result)
    .def_readonly("final_move", &osl::MiniRecord::final_move)
    .def("state_size", &osl::MiniRecord::state_size)
    .def("move_size", &osl::MiniRecord::move_size)
    .def("set_initial_state", &osl::MiniRecord::set_initial_state)
    .def("add_move", [](osl::MiniRecord& record, osl::Move move, bool in_check) {
      if (record.history.size() == 0)
        throw std::logic_error("add_move before set_initial_state");
      record.add_move(move, in_check);
    })
    .def("settle_repetition", &osl::MiniRecord::settle_repetition)
    .def("branch_at", &osl::MiniRecord::branch_at)
    .def("repeat_count", &osl::MiniRecord::repeat_count, py::arg("id")=0)
    .def("previous_repeat_index", &osl::MiniRecord::previous_repeat_index, py::arg("id")=0)
    .def("consecutive_in_check", &osl::MiniRecord::consecutive_in_check, py::arg("id")=0)
    .def("has_winner", &osl::MiniRecord::has_winner)
    .def("to_usi", py::overload_cast<const osl::MiniRecord&>(&osl::to_usi))
    .def("pack_record", [](const osl::MiniRecord& r){
      std::vector<uint64_t> code; osl::bitpack::append_binary_record(r, code);
      return code;
    }, "encode in uint64 array")
    .def("export_all", &osl::MiniRecord::export_all, py::arg("flip_if_white_to_move")=true)
    .def("__len__", [](const osl::MiniRecord& r) { return r.moves.size(); })
    .def("__repr__", [](const osl::MiniRecord& r) {
      return "<MiniRecord '"+osl::to_usi(r.initial_state)
        + " " + std::to_string(r.moves.size()) + " moves'>"; })
    .def(py::self == py::self)
    .def(py::self != py::self)
    .def("__copy__",  [](const osl::MiniRecord& r) { return osl::MiniRecord(r);})
    .def("__deepcopy__",  [](const osl::MiniRecord& r) { return osl::MiniRecord(r);})
    ;
  // minor classes 
  py::class_<osl::StateRecord256>(m, "StateRecord256", py::dynamic_attr())
    .def_readonly("state", &osl::StateRecord256::state)
    .def_readonly("move", &osl::StateRecord256::next)
    .def_readonly("result", &osl::StateRecord256::result)
    .def_readonly("flipped", &osl::StateRecord256::flipped)
    .def("to_bitset", &osl::StateRecord256::to_bitset)
    .def("restore", &osl::StateRecord256::restore)
    .def("__copy__",  [](const osl::StateRecord256& r) { return osl::StateRecord256(r);})
    .def("__deepcopy__",  [](const osl::StateRecord256& r) { return osl::StateRecord256(r);})
    ;  
  
  // functions
  typedef osl::EffectState state_t;
  m.def("alt", [](osl::Player p){ return osl::alt(p); }, "alternative player color");
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
  m.def("to_csa", py::overload_cast<osl::Ptype>(&osl::to_csa));
  m.def("to_csa", py::overload_cast<osl::Player>(&osl::to_csa));
  m.def("to_ja", py::overload_cast<osl::Square>(&osl::to_ki2));
  m.def("to_ja", py::overload_cast<osl::Ptype>(&osl::to_ki2));
  m.def("to_ja", py::overload_cast<osl::Move, const state_t&, osl::Square>(&osl::to_ki2),
        py::arg("move"), py::arg("state"), py::arg("prev_to")=osl::Square());
  m.def("to_state_label_tuple", [](std::array<uint64_t,4> binary){
    osl::StateRecord256 obj;
    obj.restore(binary);
    return obj;
  });
  
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
    .value("Draw", osl::Draw).value("InGame", osl::InGame)
    .export_values();

  // data
  m.attr("ptype_move_direction") = &osl::ptype_move_direction;
  m.attr("ptype_piece_id") = &osl::ptype_piece_id;
  m.attr("ptype_csa_names") = &osl::ptype_csa_names;
  m.attr("ptype_en_names") = &osl::ptype_en_names;
  m.attr("piece_stand_order") = &osl::piece_stand_order;
  
  // "mapping of ptype to bitset of movable directions"

}

