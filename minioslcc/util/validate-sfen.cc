// validate-sfen.cc
#include "record.h"
#include "impl/bitpack.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <ranges>

bool validate_library_as_well = true;
int checkmate_success = 0;

void test_checkmate1ply(const osl::EffectState& src) {
  if (src.inCheck())
    return;
  static osl::EffectState state;
  state.copyFrom(src);
  state.makeMovePass();
  auto check = state.tryCheckmate1ply();
  if (! check.isNormal())
    return;
  if (! state.isLegal(check))
    throw std::logic_error("illegal checkmate move");
  
  state.makeMove(check);
  if (! state.inCheckmate())
    throw std::logic_error("checkmate failed");
  ++checkmate_success;
}

void check_consistency(const osl::MiniRecord& record) {
  auto state = record.initial_state;
  bool made_check = false, made_checkmate = false;
  osl::MoveVector all, check;
  int cnt = 0;
  auto last_to = osl::Square();
  for (auto move: record.moves) {
    if (made_checkmate)
      throw std::logic_error("checkmate inconsistent");
    if (state.inCheck(alt(state.turn())))
      throw std::runtime_error("check escape fail");
    if (state.inCheck() != made_check)
      throw std::logic_error("check inconsistent");
    // state.generateLegal(all);
    state.generateWithFullUnpromotions(all);
    if (std::ranges::find(all, move) == all.end()) {
      std::cerr << all << '\n';
      throw std::runtime_error("movegen "+to_usi(move)+" "+std::to_string(cnt));
    }
    made_check = state.isCheck(move);
    if (made_check) {
      state.generateCheck(check);
      if (std::ranges::find(check, move) == check.end()) {
        if (! move.ignoreUnpromote())
          throw std::runtime_error("movegen check "+to_usi(move)+" "+std::to_string(cnt));
        //std::cerr << "ignore unpromote in check\n";
      }
    }
    test_checkmate1ply(state);

    auto ja = to_ki2(move, state, last_to);
    auto m = osl::kanji::to_move(ja, state, last_to);
    if (move != m)
      throw std::logic_error("japanese representation for "+to_csa(move));
    
    state.makeMove(move);
    last_to = move.to();
    
    if (!state.check_internal_consistency())
      throw std::runtime_error("internal consistency "+to_usi(move)+" "+std::to_string(cnt));
    made_checkmate = state.inCheckmate();
    
    ++cnt;
  }
}

int main(int argc, char *argv[]) {
  using namespace std::string_literals;
  if (argc != 2 || argv[1][0] == '-') {
    std::cout << "usage: argv[0] sfen-file-name\n";
    return (argc >= 2 && argv[1] == "--help"s) ? 0 : 1;
  }

  auto sfen = std::filesystem::path(argv[1]);
  if (! exists(sfen)) {
    std::cerr << "file not found\n";
    return 1;
  }
  
  std::ifstream is(sfen);
  int count=0, repetition_count=0, repetition_draw=0, declaration_count=0;
  std::string line;
  try {
    while (getline(is, line)) {
      auto record=osl::usi::read_record(line); // move validity
      if (validate_library_as_well)
        check_consistency(record);
      ++count;
      if (record.repeat_count() >= 3) {
        ++repetition_count;
        if (record.result == osl::Draw)
          ++repetition_draw;
      }
      if (record.final_move == osl::Move::DeclareWin())
        ++declaration_count;
    }
  }
  catch (std::exception& e) {
    std::cerr << "error at line " << count << " " << line << "\n";
    std::cerr << e.what();
    return 1;
  }
  std::cout << "read " << count << " records\n";
  std::cout << "1ply checkmate " << checkmate_success << "\n"
            << "draw by repetition " << repetition_draw << "\n"
            << "other repetition " << repetition_count - repetition_draw << "\n"
            << "win by declaration " << declaration_count << "\n"
    ;
}
