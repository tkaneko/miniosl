#include "pyb/miniosl.h"
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/operators.h>
#include <pybind11/stl/filesystem.h>

#include "state.h"
#include "record.h"
#include "opening.h"
#include "feature.h"
#include "impl/bitpack.h"
#include "impl/more.h"
#include <sstream>
#include <iostream>
#include <fstream>

PYBIND11_MODULE(minioslcc, m) {
  m.doc() = "shogi utilities derived from osl.  all members should be incorporated inside :py:mod:`miniosl` module";
  pyosl::init_basic(m);
  pyosl::init_state_np(m);
  pyosl::init_game(m);
}

void pyosl::init_basic(py::module_& m) {
  using namespace pybind11::literals;

  // classes
  py::class_<osl::Square>(m, "Square", py::dynamic_attr(), "square (x, y) with onboard range in (1, 1) to (9, 9) and with some invalid ranges outside the board for sentinels and piece stand.\n\n>>> sq = miniosl.Square(2, 6)\n>>> sq.x()\n2\n>>> sq.y()\n6\n>>> sq.is_onboard()\nTrue\n>>> sq.to_xy()\n(2, 6)")
    .def(py::init<>())
    .def(py::init<int,int>())
    .def("x", &osl::Square::x, "in [1,9]")
    .def("y", &osl::Square::y, "in [1,9]")
    .def("to_xy", [](osl::Square sq) { return std::make_pair<int,int>(sq.x(), sq.y()); })
    .def("to_usi", [](osl::Square sq) { return osl::to_psn(sq); })
    .def("to_csa", [](osl::Square sq) { return osl::to_csa(sq); })
    .def("is_onboard", &osl::Square::isOnBoard)
    .def("is_piece_stand", &osl::Square::isPieceStand)
    .def("is_promote_area", &osl::Square::isPromoteArea, "color"_a, "test this is promote area for `color`\n\n"
         ">>> miniosl.Square(1, 3).is_promote_area(miniosl.black)\n"
         "True\n"
         ">>> miniosl.Square(1, 3).is_promote_area(miniosl.white)\n"
         "False\n"
         ">>> miniosl.Square(5, 5).is_promote_area(miniosl.black)\n"
         "False\n"
         )
    .def("rotate180", &osl::Square::rotate180, "return a square after rotation.\n\n>>> sq = miniosl.Square(2, 6)\n>>> sq.rotate180().to_xy()\n(8, 4)")
    .def("index81", py::overload_cast<>(&osl::Square::index81, py::const_), "return index in range [0, 80]")
    .def("__repr__", [](osl::Square sq) { return "<Square '"+osl::to_psn(sq) + "'>"; })
    .def("__str__", [](osl::Square sq) { return osl::to_csa(sq); })
    .def(py::self == py::self)
    .def(py::self != py::self)
    ;
  py::class_<osl::Move>(m, "Move", py::dynamic_attr(), "move in shogi.\n\ncontaining the source and destination positions, moving piece, and captured one if any\n\n"
                        ">>> shogi = miniosl.UI()\n"
                        ">>> move = shogi.to_move('+7776FU')\n"
                        ">>> move.src() == miniosl.Square(7, 7)\n"
                        "True\n"
                        ">>> move.dst() == miniosl.Square(7, 6)\n"
                        "True\n"
                        ">>> move.ptype() == miniosl.pawn\n"
                        "True\n"
                        ">>> move.color() == miniosl.black\n"
                        "True\n"
                        ">>> move.is_drop()\n"
                        "False\n"
                        ">>> move.is_capture()\n"
                        "False\n"
                        ">>> move.to_usi()\n"
                        "'7g7f'\n"
                        )
    .def("src", &osl::Move::from)
    .def("dst", &osl::Move::to)
    .def("ptype", &osl::Move::ptype, "piece type after move")
    .def("old_ptype", &osl::Move::oldPtype, "piece type before move")
    .def("capture_ptype", &osl::Move::capturePtype)
    .def("is_promotion", &osl::Move::isPromotion)
    .def("is_drop", &osl::Move::isDrop)
    .def("is_normal", &osl::Move::isNormal)
    .def("is_capture", &osl::Move::isCapture)
    .def("color", &osl::Move::player)
    .def("rotate180", &osl::Move::rotate180)
    .def("to_usi", [](osl::Move m) { return osl::to_usi(m); })
    .def("to_csa", [](osl::Move m) { return osl::to_csa(m); })
    .def("policy_move_label", &osl::ml::policy_move_label, "move index for cross-entropy loss in training")
    .def("__repr__", [](osl::Move m) { return "<Move '"+osl::to_psn(m) + "'>"; })
    .def("__str__", [](osl::Move m) { return osl::to_csa(m); })
    .def("__hash__", [](osl::Move m) { return m.intValue(); })
    .def(py::self == py::self)
    .def(py::self != py::self)
    .def(py::self < py::self)
    .def_static("resign", &osl::Move::Resign, "return resign")
    .def_static("declare_win", &osl::Move::DeclareWin, "return win declaration")
    ;
  py::class_<osl::Piece>(m, "Piece", py::dynamic_attr(), "a state of piece placed in a corresponding :py:class:`BaseState`")
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
  
  py::class_<osl::MiniRecord>(m, "MiniRecord", py::dynamic_attr(), "a game record")
    .def(py::init<>())
    .def_readonly("initial_state", &osl::MiniRecord::initial_state)
    .def_readonly("moves", &osl::MiniRecord::moves, "list of :py:class:`Move` s")
    .def_readonly("result", &osl::MiniRecord::result, ":py:class:`GameResult`")
    .def_readonly("final_move", &osl::MiniRecord::final_move, "resign or win declaration in :py:class:`Move`")
    .def("state_size", &osl::MiniRecord::state_size)
    .def("move_size", &osl::MiniRecord::move_size, "#moves played so far")
    .def("set_initial_state", &osl::MiniRecord::set_initial_state, "state"_a, ":meta private:")
    .def("append_move", [](osl::MiniRecord& record, osl::Move move, bool in_check) {
      if (record.history.size() == 0)
        throw std::logic_error("add_move before set_initial_state");
      record.append_move(move, in_check);
    }, "move"_a, "in_check"_a, ":meta private: append a new move")
    .def("settle_repetition", &osl::MiniRecord::settle_repetition, ":meta private:")
    .def("branch_at", &osl::MiniRecord::branch_at, "id"_a, ":meta private:")
    .def("repeat_count", &osl::MiniRecord::repeat_count, "id"_a=0)
    .def("previous_repeat_index", &osl::MiniRecord::previous_repeat_index, "id"_a=0)
    .def("consecutive_in_check", &osl::MiniRecord::consecutive_in_check, "id"_a=0)
    .def("has_winner", &osl::MiniRecord::has_winner)
    .def("to_usi", py::overload_cast<const osl::MiniRecord&>(&osl::to_usi), "export to usi string")
    .def("pack_record", [](const osl::MiniRecord& r){
      std::vector<uint64_t> code; osl::bitpack::append_binary_record(r, code);
      return code;
    }, "encode in uint64 array")
    .def("export_all", &osl::MiniRecord::export_all, "flip_if_white_to_move"_a=true,
         "make a sequence of training records each of which has 32 byets")
    .def("export_all320", &osl::MiniRecord::export_all320, "flip_if_white_to_move"_a=true,
         "make a sequence of training records each of which has 40 byets")
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
  py::class_<osl::RecordSet>(m, "RecordSet", py::dynamic_attr())
    .def(py::init<>())
    .def(py::init<const std::vector<osl::MiniRecord>&>())
    .def_readonly("records", &osl::RecordSet::records, "list of :py:class:`MiniRecord` s")
    .def_static("from_usi_file", &osl::RecordSet::from_usi_file, "path"_a, "read usi lines")
    .def("__len__", [](const osl::RecordSet& r) { return r.records.size(); })
    ;
  py::class_<osl::BasicHash>(m, "BasicHash", py::dynamic_attr(), "hash code for a state")
    .def(py::init<>())
    .def(py::self == py::self)
    .def(py::self != py::self)
    ;
  auto tree_type = py::class_<osl::OpeningTree>(m, "OpeningTree", py::dynamic_attr())
    .def(py::init<>())
    .def_readonly("table", &osl::OpeningTree::table)
    .def("__contains__", [](osl::OpeningTree& t, std::pair<uint64_t, uint32_t> key) { return t.contains(key); })
    .def("__getitem__", [](osl::OpeningTree& t, std::pair<uint64_t, uint32_t> key) { return t[key]; })
    .def("size", &osl::OpeningTree::size)
    .def("board_size", &osl::OpeningTree::board_size)
    .def("export_all", &osl::OpeningTree::export_all)
    .def_static("from_record_set", &osl::OpeningTree::from_record_set, "data"_a, "minimum_count"_a)
    .def_static("restore_from", &osl::OpeningTree::restore_from, "board_code"_a, "stand_code"_a, "node_code"_a,
                "restore from data generated by :py:meth:`export_all`")
    ;
  py::class_<osl::OpeningTree::Node>(tree_type, "Node", py::dynamic_attr())
    .def_readonly("result_count", &osl::OpeningTree::Node::result_count, "list of int for all :py:class:`GameResult`")
    .def("__getitem__", &osl::OpeningTree::Node::operator[])
    .def("count", &osl::OpeningTree::Node::count)
    .def("black_advantage", &osl::OpeningTree::Node::black_advantage)
    .def("__str__", [](const osl::OpeningTree::Node& node) {
      return std::to_string(node[0]) + "-" + std::to_string(node[1]) + "-" + std::to_string(node[2])
        + " (" + std::to_string(node[3]) + ")"; })
    .def("__repr__", [](const osl::OpeningTree::Node& node) {
      return "<OpeningTree.Node "+std::to_string(node.count()) +">"; })
    ;
  
  // functions
  typedef osl::EffectState state_t;
  m.def("alt", py::overload_cast<osl::Player>(osl::alt), "color"_a, "alternative player color\n\n>>> miniosl.alt(miniosl.black) == miniosl.white\nTrue");
  m.def("sign", py::overload_cast<osl::Player>(osl::sign), "color"_a, "+1 for black or -1 for white");
  m.def("csa_board", [](std::string input){
    try { return osl::csa::read_board(input); }
    catch (std::exception& e) { std::cerr << e.what() << '\n'; } return state_t();
  }, "state_string"_a, "parse and return State");
  m.def("usi_board", [](std::string input){
    auto record = osl::usi::read_record(input); return record.initial_state;
  }, "state_string"_a, "parse and return State");
  m.def("csa_record", py::overload_cast<std::string>(&osl::csa::read_record), "record_string"_a, "read str as a game record");
  m.def("csa_file", [](std::string filepath){
    return osl::csa::read_record(std::filesystem::path(filepath)); }, "path"_a, "load a game record");
  m.def("usi_record", &osl::usi::read_record, "record_string"_a, "read str as a game record");
  m.def("usi_sub_record", [](std::string s){ auto ret = osl::usi::read_record(s); return osl::SubRecord(ret); },
        "record_string"_a, "read str as a game record in SubRecord");
  m.def("usi_file", [](std::string filepath, int id=0){
    std::ifstream is(filepath);
    std::string line;
    for (int i=0; i<id; ++i) getline(is, line);
    getline(is, line);
    return osl::usi::read_record(line); }, "path"_a, "line_id"_a=0, "load a game record");
  m.def("kif_file", py::overload_cast<const std::filesystem::path&>(&osl::kifu::read_record), "filepath"_a);
  m.def("to_csa", py::overload_cast<osl::Ptype>(&osl::to_csa));
  m.def("to_csa", py::overload_cast<osl::Player>(&osl::to_csa));
  m.def("to_ja", py::overload_cast<osl::Square>(&osl::to_ki2));
  m.def("to_ja", py::overload_cast<osl::Ptype>(&osl::to_ki2));
  m.def("to_ja", py::overload_cast<osl::Move, const state_t&, osl::Square>(&osl::to_ki2),
        "move"_a, "state"_a, "prev_to"_a=osl::Square());
  m.def("hash_after_move", &osl::make_move);
  m.def("win_result", &osl::win_result);

  // enums
  py::enum_<osl::Player>(m, "Player", py::arithmetic())
    .value("black", osl::BLACK, "first player").value("white", osl::WHITE, "second player")
    .export_values();
  py::enum_<osl::Ptype>(m, "Ptype", py::arithmetic(), "piece type")
    .value("pawn",   osl::PAWN).value("lance",  osl::LANCE).value("knight", osl::KNIGHT)
    .value("silver", osl::SILVER).value("gold",   osl::GOLD).value("bishop", osl::BISHOP)
    .value("rook",   osl::ROOK).value("king",   osl::KING)
    .value("ppawn",   osl::PPAWN, "promoted pawn").value("plance",  osl::PLANCE, "promoted lance")
    .value("pknight", osl::PKNIGHT, "promoted knight").value("psilver", osl::PSILVER, "promoted silver")
    .value("pbishop", osl::PBISHOP, "promoted bishop").value("prook",   osl::PROOK, "promoted rook")
    .value("empty", osl::Ptype_EMPTY).value("edge",  osl::Ptype_EDGE)
    .export_values();
  py::enum_<osl::Direction>(m, "Direction", py::arithmetic(), "direction.\n\n"
                            ">>> direction_set = miniosl.ptype_move_direction[int(miniosl.pawn)]\n"
                            ">>> bin(direction_set)\n"
                            "'0b10'\n"
                            ">>> (direction_set & (1 << int(miniosl.U))) != 0\n"
                            "True\n"
                            )
    .value("UL", osl::UL, "up left").value("U",  osl::U).value("UR", osl::UR)
    .value("L",  osl::L).value("R",  osl::R)
    .value("DL", osl::DL).value("D",  osl::D).value("DR", osl::DR)
    .value("UUL", osl::UUL, "up up left for knight").value("UUR", osl::UUR)
    .value("Long_UL", osl::Long_UL, "UL for bishops").value("Long_U",  osl::Long_U).value("Long_UR", osl::Long_UR)
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
  m.attr("channel_id") = &osl::ml::channel_id;
  m.attr("draw_limit") = &osl::MiniRecord::draw_limit;
  m.attr("One") = &osl::ml::One;
  m.attr("input_unit") = &osl::ml::input_unit;
  m.attr("aux_unit") = &osl::ml::aux_unit;
  m.attr("channels_per_history") = &osl::ml::channels_per_history;
  m.attr("history_length") = &osl::ml::history_length;
  
  // "mapping of ptype to bitset of movable directions"

  py::bind_vector<std::vector<osl::Move>>(m, "MoveVector");
  py::bind_vector<std::vector<osl::HashStatus>>(m, "HashStatusVector");
  py::bind_vector<std::vector<osl::MiniRecord>>(m, "MiniRecordVector");
}

