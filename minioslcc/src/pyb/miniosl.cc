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
  typedef osl::EffectState state_t;

  // enums
  py::enum_<osl::Player>(m, "Player", py::arithmetic())
    .value("black", osl::BLACK, "first player").value("white", osl::WHITE, "second player")
    .export_values()
    .def_property_readonly("alt", py::overload_cast<osl::Player>(osl::alt),
                           "opponent or alternative player color\n\n"
                           ":type: :py:class:`minioslcc.Player`\n\n"
                           ">>> miniosl.black.alt == miniosl.white\nTrue")
    .def("sign", py::overload_cast<osl::Player>(osl::sign), "+1 for black or -1 for white")
    .def("to_csa", py::overload_cast<osl::Player>(&osl::to_csa))
    .def("win_result", &osl::win_result)
    .def("loss_result", &osl::loss_result)
    ;
  py::enum_<osl::Ptype>(m, "Ptype", py::arithmetic(), "piece type")
    .value("pawn",   osl::PAWN).value("lance",  osl::LANCE).value("knight", osl::KNIGHT)
    .value("silver", osl::SILVER).value("gold",   osl::GOLD).value("bishop", osl::BISHOP)
    .value("rook",   osl::ROOK).value("king",   osl::KING)
    .value("ppawn",   osl::PPAWN, "promoted pawn").value("plance",  osl::PLANCE, "promoted lance")
    .value("pknight", osl::PKNIGHT, "promoted knight").value("psilver", osl::PSILVER, "promoted silver")
    .value("pbishop", osl::PBISHOP, "promoted bishop").value("prook",   osl::PROOK, "promoted rook")
    .value("empty", osl::Ptype_EMPTY).value("edge",  osl::Ptype_EDGE)
    .export_values()
    .def("promote", py::overload_cast<osl::Ptype>(&osl::promote),
         "return promoted type if legal\n\n"
         ">>> miniosl.rook.promote() == miniosl.prook\n"
         "True\n"
         ">>> miniosl.prook.promote() == miniosl.prook\n"
         "True\n"
         )
    .def("unpromote", py::overload_cast<osl::Ptype>(&osl::unpromote),
         "return the original type if promoted\n\n"
         ">>> miniosl.prook.unpromote() == miniosl.rook\n"
         "True\n"
         )
    .def("is_piece", py::overload_cast<osl::Ptype>(&osl::is_piece),
         "True (False) is a normal piece type (empty or edge)\n\n"
         ">>> miniosl.rook.is_piece()\n"
         "True\n"
         ">>> miniosl.empty.is_piece()\n"
         "False\n"
         )
    .def("to_csa", py::overload_cast<osl::Ptype>(&osl::to_csa),
         "return csa representation\n\n"
         ">>> miniosl.rook.to_csa()\n"
         "'HI'\n"
         )
    .def("to_ja", py::overload_cast<osl::Ptype>(&osl::to_ki2),
         "return Japanese representation\n\n"
         ">>> miniosl.pawn.to_ja()\n"
         "'歩'\n"
         )
    // `miniosl.pawn.name` -> ppawn
    // .def("to_en", [](osl::Ptype ptype) { return osl::ptype_en_names[idx(ptype)]; })
    .def_property_readonly("count", [](osl::Ptype ptype) { return osl::ptype_piece_count(ptype); },
                           "number of pieces in the standard rule")
    .def_property_readonly("has_long_move", &osl::ptype_has_long_move, "True if lance, bishop, rook, pbishop, or prook\n\n"
                           ":type: bool")
    .def_property_readonly("one_hot", py::overload_cast<osl::Ptype>(&osl::one_hot),
                           "building block for bitset\n\n"
                           ":type: int")
    .def_property_readonly("direction_set",
                           [](osl::Ptype ptype) { return osl::ptype_move_direction[idx(ptype)]; },
                           "bitset of :py:class:`Direction` indicating legal moves\n\n"
                           ">>> miniosl.pawn.direction_set == miniosl.U.one_hot\n"
                           "True\n"
                           )
    .def_property_readonly("move_type", [](osl::Ptype ptype) { return osl::ptype_move_type[idx(ptype)]; },
                           "return gold if move type is equivalent\n\n"
                           ":type: :py:class:`minioslcc.Ptype`\n\n"
                           ">>> miniosl.pawn.move_type == miniosl.gold\n"
                           "False\n"
                           ">>> miniosl.ppawn.move_type == miniosl.gold\n"
                           "True\n"
                           )
    .def_property_readonly("piece_id", [](osl::Ptype ptype) { return osl::ptype_piece_id[idx(ptype)]; },
                           "range of piece id [0,39] assigned to the ptype\n\n"
                           ">>> miniosl.rook.piece_id\n"
                           "(38, 40)\n"
                           )
    .def_property_readonly("piece_id_set", &osl::piece_id_set, "bitset of piece_id\n\n"
                           ">>> miniosl.rook.piece_id_set\n"
                           "824633720832\n"
                           ">>> miniosl.rook.piece_id_set == ((1<<38) | (1<<39))\n"
                           "True\n"
                           )
    ;
  py::enum_<osl::Direction>(m, "Direction", py::arithmetic(), "direction.\n\n"
                            ">>> direction_set = miniosl.pawn.direction_set\n"
                            ">>> bin(direction_set)\n"
                            "'0b10'\n"
                            ">>> (direction_set & miniosl.U.one_hot) != 0\n"
                            "True\n"
                            )
    .value("UL", osl::UL, "up left").value("U",  osl::U).value("UR", osl::UR)
    .value("L",  osl::L).value("R",  osl::R)
    .value("DL", osl::DL).value("D",  osl::D).value("DR", osl::DR)
    .value("UUL", osl::UUL, "up of up left for knight").value("UUR", osl::UUR)
    .value("Long_UL", osl::Long_UL, "UL for bishops").value("Long_U",  osl::Long_U).value("Long_UR", osl::Long_UR)
    .value("Long_L",  osl::Long_L).value("Long_R",  osl::Long_R)
    .value("Long_DL", osl::Long_DL).value("Long_D",  osl::Long_D).value("Long_DR", osl::Long_DR)
    .export_values()
    .def("is_long", &osl::is_long,
         "True if long direction\n\n"
         ">>> miniosl.U.is_long()\n"
         "False\n"
         ">>> miniosl.Long_U.is_long()\n"
         "True\n"
         )
    .def("is_base8", &osl::is_base8,
         "True if eight neighbors\n\n"
         ">>> miniosl.UL.is_base8()\n"
         "True\n"
         ">>> miniosl.UUL.is_base8()\n"
         "False\n"
         )
    .def("to_base8", &osl::long_to_base8,
         "make corresponding base8 direction from long one\n\n"
         ">>> miniosl.Long_U.to_base8() == miniosl.U\n"
         "True\n"
         )
    .def("to_long", &osl::to_long,
         "make corresponding long direction from base8\n\n"
         ">>> miniosl.U.to_long() == miniosl.Long_U\n"
         "True\n"
         )
    .def_property_readonly("inverse", &osl::inverse,
         "make inverse direction\n\n"
         ">>> miniosl.UL.inverse == miniosl.DR\n"
         "True\n"
         ">>> miniosl.Long_D.inverse == miniosl.Long_U\n"
         "True\n"
         ">>> miniosl.UUL.inverse == miniosl.UUL  # DDR is not defined\n"
         "True\n"
         )
    .def_property_readonly("one_hot", py::overload_cast<osl::Direction>(&osl::one_hot),
                           "building block for bitset\n\n"
                           ":type: int")
    .def_property_readonly("black_dx", &osl::black_dx,
                           "delta x in black's view\n\n"
                           ":type: int")
    .def_property_readonly("black_dy", &osl::black_dy,
                           "delta y in black's view\n\n"
                           ":type: int")
    .def("to_offset", [](osl::Direction dir, osl::Player c){ return osl::to_offset(c, dir); },
         "color"_a=osl::BLACK,
         "obtain move offset for color\n\n"
         ">>> sq, dir = miniosl.Square(7, 7), miniosl.U\n"
         ">>> sq.add(dir.to_offset()) == miniosl.Square(7, 6)\n"
         "True\n"
         ">>> sq.add(dir.to_offset(miniosl.white)) == miniosl.Square(7, 8)\n"
         "True\n"
         )
    ;
  py::enum_<osl::Offset>(m, "Offset", py::arithmetic(), "relative square location")
    ;

  py::enum_<osl::GameResult>(m, "GameResult")
    .value("BlackWin", osl::BlackWin).value("WhiteWin", osl::WhiteWin)
    .value("Draw", osl::Draw).value("InGame", osl::InGame)
    .export_values()
    .def("flip", &osl::flip, "invert winner")
    .def("has_winner", &osl::has_winner)
    ;

  // classes
  py::class_<osl::Square>(m, "Square", py::dynamic_attr(),
                          "square (x, y) with onboard range in (1, 1) to (9, 9) and with some invalid ranges outside the board for sentinels and piece stand.\n\n"
                          ">>> sq = miniosl.Square(2, 6)\n"
                          ">>> sq.x\n"
                          "2\n"
                          ">>> sq.y\n"
                          "6\n"
                          ">>> sq.is_onboard()\n"
                          "True\n"
                          ">>> sq.to_xy()\n"
                          "(2, 6)")
    .def(py::init<>())
    .def(py::init<int,int>())
    .def_property_readonly("x", &osl::Square::x, "int in [1, 9]")
    .def_property_readonly("y", &osl::Square::y, "int in [1, 9]")
    .def("to_xy", [](osl::Square sq) { return std::make_pair<int,int>(sq.x(), sq.y()); })
    .def("to_usi", [](osl::Square sq) { return osl::to_psn(sq); })
    .def("to_csa", [](osl::Square sq) { return osl::to_csa(sq); })
    .def("to_ja", [](osl::Square sq) { return osl::to_ki2(sq); }, "Japanese label")
    .def("is_onboard", &osl::Square::isOnBoard)
    .def("is_piece_stand", &osl::Square::isPieceStand)
    .def("is_promote_area", &osl::Square::isPromoteArea, "color"_a=osl::BLACK,
         "test this is in promote area for `color`\n\n"
         ">>> miniosl.Square(1, 3).is_promote_area(miniosl.black)\n"
         "True\n"
         ">>> miniosl.Square(1, 3).is_promote_area()  # default for black\n"
         "True\n"
         ">>> miniosl.Square(1, 3).is_promote_area(miniosl.white)\n"
         "False\n"
         ">>> miniosl.Square(5, 5).is_promote_area(miniosl.black)\n"
         "False\n"
         )
    .def("rotate180", &osl::Square::rotate180,
         "return a square after rotation.\n\n"
         ">>> sq = miniosl.Square(2, 6)\n"
         ">>> sq.rotate180().to_xy()\n"
         "(8, 4)")
    .def("index81", py::overload_cast<>(&osl::Square::index81, py::const_), "return index in range [0, 80]")
    .def("neighbor",
         [](osl::Square sq, osl::Direction dir, osl::Player color){
           return sq + osl::to_offset(color, dir);
         },
         "dir"_a, "color"_a=osl::BLACK, "return a neighbor for direction\n\n\n"
         ">>> miniosl.Square(7, 7).neighbor(miniosl.U) == miniosl.Square(7, 6)\n"
         "True\n"
         )
    .def("add", [](osl::Square sq, osl::Offset o){ return sq + o; },
         "offset"_a, "[advanced] return a square with location offset\n\n\n"
         ">>> miniosl.Square(7, 7).add(miniosl.U.to_offset()) == miniosl.Square(7, 6)\n"
         "True\n"
         )
    .def("__repr__", [](osl::Square sq) { return "<Square '"+osl::to_psn(sq) + "'>"; })
    .def("__str__", [](osl::Square sq) { return osl::to_csa(sq); })
    .def(py::self == py::self)
    .def(py::self != py::self)
    ;
  py::class_<osl::Move>(m, "Move", py::dynamic_attr(), "move in shogi.\n\ncontaining the source and destination positions, moving piece, and captured one if any\n\n"
                        ">>> shogi = miniosl.UI()\n"
                        ">>> move = shogi.to_move('+7776FU')\n"
                        ">>> move.src == miniosl.Square(7, 7)\n"
                        "True\n"
                        ">>> move.dst == miniosl.Square(7, 6)\n"
                        "True\n"
                        ">>> move.ptype == miniosl.pawn\n"
                        "True\n"
                        ">>> move.color == miniosl.black\n"
                        "True\n"
                        ">>> move.is_drop()\n"
                        "False\n"
                        ">>> move.is_capture()\n"
                        "False\n"
                        ">>> move.to_usi()\n"
                        "'7g7f'\n"
                        )
    .def_property_readonly("src", &osl::Move::from, "departed :py:class:`Square` (can be piece stand)")
    .def_property_readonly("dst", &osl::Move::to, "arrived :py:class:`Square`")
    .def_property_readonly("ptype", &osl::Move::ptype, "piece :py:class:`Ptype` after move")
    .def_property_readonly("old_ptype", &osl::Move::oldPtype, "piece :py:class:`Ptype` before move")
    .def_property_readonly("capture_ptype", &osl::Move::capturePtype,
                           "captured :py:class:`Ptype` by move")
    .def("is_promotion", &osl::Move::isPromotion)
    .def("is_drop", &osl::Move::isDrop)
    .def("is_normal", &osl::Move::isNormal)
    .def("is_capture", &osl::Move::isCapture)
    .def_property_readonly("color", &osl::Move::player,
                           ":py:class:`Player` of move")
    .def("rotate180", &osl::Move::rotate180)
    .def("to_usi", [](osl::Move m) { return osl::to_usi(m); })
    .def("to_csa", [](osl::Move m) { return osl::to_csa(m); })
    .def("to_ja",
         [](osl::Move m, const state_t& state, std::optional<osl::Square> prev_to) {
           return osl::to_ki2(m, state, prev_to.value_or(osl::Square()));
         },
      "state"_a, "prev_to"_a=std::nullopt,
      "Japanese for human")
    .def_property_readonly("policy_move_label", &osl::ml::policy_move_label,
                           "move index in `int` for cross-entropy loss in training")
    .def("__repr__", [](osl::Move m) { return "<Move '"+osl::to_psn(m) + "'>"; })
    .def("__str__", [](osl::Move m) { return osl::to_csa(m); })
    .def("__hash__", [](osl::Move m) { return m.intValue(); })
    .def(py::self == py::self)
    .def(py::self != py::self)
    .def(py::self < py::self)
    .def_static("resign", &osl::Move::Resign, "return resign")
    .def_static("declare_win", &osl::Move::DeclareWin, "return win declaration")
    ;
  py::class_<osl::Piece>(m, "Piece", py::dynamic_attr(),
                         "a state of piece placed in a corresponding :py:class:`BaseState`")
    .def_property_readonly("square", &osl::Piece::square, ":py:class:`Square`")
    .def_property_readonly("ptype", &osl::Piece::ptype, ":py:class:`Ptype`")
    .def_property_readonly("color", &osl::Piece::owner, ":py:class:`Player`")
    .def("is_piece", &osl::Piece::isPiece)
    .def("has",
         [](osl::Piece piece, osl::Ptype ptype, osl::Player color) {
           return piece.ptype() == ptype && piece.owner() == color;
         }, "ptype"_a, "color"_a,
         "test if piece has a specified piece type with color")
    .def("equals", &osl::Piece::equalPtyeO,
         "equality w.r.t. PtypeO (i.e., ignoring piece id or location)"
         )
    .def_property_readonly("id", &osl::Piece::id, "id in [0,39]\n\n"
                           ":type: int")
    .def("__repr__", [](osl::Piece p) {
      std::stringstream ss;
      ss << p;
      return "<Piece '"+ss.str()+ "'>";
    })
    .def(py::self == py::self)
    .def(py::self != py::self)
    ;
  
  py::class_<osl::MiniRecord>(m, "MiniRecord", py::dynamic_attr(), "a game record\n\n"
                              ">>> record = miniosl.usi_record('startpos moves 7g7f')\n"
                              ">>> record.move_size()\n"
                              "1\n"
                              ">>> record.moves[0].to_usi()\n"
                              "'7g7f'\n"
                              ">>> s = record.replay(-1)\n"
                              ">>> s.piece_at(miniosl.Square(7, 6)).ptype == miniosl.pawn\n"
                              "True\n"
                              )
    .def(py::init<>())
    .def(py::init<const osl::MiniRecord&>())
    .def_readonly("initial_state", &osl::MiniRecord::initial_state)
    .def_readonly("moves", &osl::MiniRecord::moves, "list of :py:class:`Move` s")
    .def_readonly("result", &osl::MiniRecord::result, ":py:class:`GameResult`")
    .def_readonly("final_move", &osl::MiniRecord::final_move, "resign or win declaration in :py:class:`Move`")
    .def("state_size", &osl::MiniRecord::state_size)
    .def("move_size", &osl::MiniRecord::move_size, "#moves played so far")
    .def("set_initial_state", &osl::MiniRecord::set_initial_state,
         "state"_a,
         "shogi816k_id"_a=std::nullopt,
         ":meta private:")
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
    .def("__len__", [](const osl::MiniRecord& r) { return r.moves.size(); })
    .def("__repr__", [](const osl::MiniRecord& r) {
      return "<MiniRecord '"+osl::to_usi(r.initial_state)
        + " " + std::to_string(r.moves.size()) + " moves'>"; })
    .def(py::self == py::self)
    .def(py::self != py::self)
    .def("__copy__",  [](const osl::MiniRecord& r) { return osl::MiniRecord(r);})
    .def("__deepcopy__",  [](const osl::MiniRecord& r, py::dict) { return osl::MiniRecord(r);},
         "memo"_a)
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
  m.def("csa_board", [](std::string input){
    try { return osl::csa::read_board(input); }
    catch (std::exception& e) { std::cerr << e.what() << '\n'; } return state_t();
  }, "state_string"_a, "parse and return State");
  m.def("usi_board", [](std::string input){
    auto record = osl::usi::read_record(input); return record.initial_state;
  }, "state_string"_a, "parse and return State");
  m.def("csa_record", py::overload_cast<std::string>(&osl::csa::read_record), "record_string"_a, "read str as a game record");
  m.def("csa_file", [](std::string filepath){
    return osl::csa::read_record(std::filesystem::path(filepath)); }, "path"_a,
    "load a game record");
  m.def("usi_record", &osl::usi::read_record, "record_string"_a, "read str as a game record");
  m.def("usi_sub_record",
        [](std::string s){ auto ret = osl::usi::read_record(s); return osl::SubRecord(ret); },
        "record_string"_a, "read str as a game record in SubRecord");
  m.def("usi_file", [](std::string filepath, int id=0){
    std::ifstream is(filepath);
    std::string line;
    for (int i=0; i<id; ++i) getline(is, line);
    getline(is, line);
    return osl::usi::read_record(line); }, "path"_a, "line_id"_a=0, "load a game record");
  m.def("kif_file",
        py::overload_cast<const std::filesystem::path&>(&osl::kifu::read_record),
        "filepath"_a,
        "load a game record");
  m.def("hash_after_move", &osl::make_move);
  m.def("win_result", &osl::win_result);
  m.def("is_debug_build", [](){
#ifdef NDEBUG
    return false;
#endif
    return true;
  }, "True if assertions in minioslcc is enabled");

  // data
  m.attr("ptype_piece_id") = &osl::ptype_piece_id;
  m.attr("piece_stand_order") = &osl::piece_stand_order;
  m.attr("channel_id") = &osl::ml::channel_id;
  m.attr("draw_limit") = &osl::MiniRecord::draw_limit;
  m.attr("One") = &osl::ml::One;
  m.attr("input_unit") = &osl::ml::input_unit;
  m.attr("policy_unit") = &osl::ml::policy_unit;
  m.attr("aux_unit") = &osl::ml::aux_unit;
  m.attr("legalmove_bs_sz") = &osl::ml::legalmove_bs_sz;
  m.attr("channels_per_history") = &osl::ml::channels_per_history;
  m.attr("history_length") = &osl::ml::history_length;
  
  // "mapping of ptype to bitset of movable directions"

  py::bind_vector<std::vector<osl::Move>>(m, "MoveVector");
  py::bind_vector<std::vector<osl::HashStatus>>(m, "HashStatusVector");
  py::bind_vector<std::vector<osl::MiniRecord>>(m, "MiniRecordVector");
}

