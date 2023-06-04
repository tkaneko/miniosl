// validate-sfen.cc
#include "record.h"
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
  osl::StateLabelTuple ps2;
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

    osl::StateLabelTuple instance{state, move};
    auto bs = instance.to_bitset();
    ps2.restore(bs);
      
    if (instance.state != ps2.state || move != ps2.next)
      throw std::runtime_error("pack position consistency"+to_usi(state)+" "+to_usi(move)+" "+to_usi(ps2.next)
                               +" "+std::to_string(cnt));
    
    state.makeMove(move);
    if (!state.isConsistent())
      throw std::runtime_error("internal consistency "+to_usi(move)+" "+std::to_string(cnt));
    made_checkmate = state.inCheckmate();
    
    ++cnt;
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2 || argv[1][0] == '-') {
    std::cout << "usage: argv[0] sfen-file-name\n";
    return (argc >= 2 && std::string(argv[1]) == "--help") ? 0 : 1;
  }

  auto sfen = std::filesystem::path(argv[1]);
  if (! exists(sfen)) {
    std::cerr << "file not found\n";
    return 1;
  }
  
  std::ifstream is(sfen);
  int count=0;
  std::string line;
  try {
    while (getline(is, line)) {
      auto record=osl::usi::read_record(line); // move validity
      if (validate_library_as_well)
        check_consistency(record);
      ++count;
    }
  }
  catch (std::exception& e) {
    std::cerr << "error at line " << count << " " << line << "\n";
    std::cerr << e.what();
    return 1;
  }
  std::cout << "read " << count << " records\n";
  std::cout << "found " << checkmate_success << " 1ply checkmate\n";
}
