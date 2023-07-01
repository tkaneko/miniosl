#include "filepath.h"
#include "feature.h"
#include "impl/bitpack.h"

using namespace osl;

void test_pack_position() {
  const auto& data = test_record_set();
  int count = 0;
  for (const auto& record: data.records) {
    if (++count > limit)
      break;
    auto state(record.initial_state);
    StateRecord256 ps2;
    int cnt = 0;
    for (auto move:record.moves) {      
      StateRecord256 ps{state, move, record.result};
      auto bs = ps.to_bitset();
      ps2.restore(bs);
      
      TEST_ASSERT(to_usi(ps.state) == to_usi(ps2.state));
      TEST_ASSERT(move == ps2.next);
      TEST_MSG("%s v.s. %s", to_usi(move).c_str(), to_usi(ps2.next).c_str());
      TEST_ASSERT(ps2.result == record.result);
      state.makeMove(move);
    }
  }
}

void test_hash() {
  const auto& data = test_record_set();
  int count = 0;
  for (const auto& record: data.records) {
    if (++count > limit)
      break;
    auto state(record.initial_state);
    HashStatus code(state);
    for (auto move: record.moves) {
      state.makeMove(move);

      HashStatus code_fresh(state);
      code = code.new_zero_history(move, state.inCheck());
      TEST_ASSERT(code_fresh == code);
    }
  }
}

void test_japanese() {
  const auto& data = test_record_set();
  int count = 0;
  for (const auto& record: data.records) {
    if (++count > limit)
      break;
    auto state(record.initial_state);
    auto last_to = Square();
    for (auto move: record.moves) {
      auto ja = to_ki2(move, state, last_to);
      try  {
        auto m = kanji::to_move(ja, state, last_to);
        TEST_CHECK(move == m);
        TEST_MSG("%s\n%s %s %s\n", to_csa(state).c_str(), to_csa(move).c_str(), kanji::debugu8(ja), to_csa(m).c_str());
        TEST_ASSERT(move == m);
      }
      catch (std::exception& e){
        std::cerr << move << '\n';
        std::cerr << state << "\n";
        std::cerr << kanji::debugu8(ja) << '\n';
        throw;
      }
      state.makeMove(move);
      last_to = move.to();
    }
  }
}

void test_policy_move_label() {
  const auto& data = test_record_set();
  int count = 0;
  MoveVector legal_moves;
  for (const auto& record: data.records) {
    if (++count > limit)
      break;
    auto state(record.initial_state);
    for (auto move: record.moves) {
      state.generateLegal(legal_moves);
      for (auto m: legal_moves) {
        int code = ml::policy_move_label(m);
        TEST_CHECK(0 <= code && code < 27*81);
        TEST_CHECK(ml::decode_move_label(code, state) == m);
      }
    }
  }
}


TEST_LIST = {
  { "japanese", test_japanese },
  { "pack_position", test_pack_position },
  { "hash", test_hash },
  { "policy_move_label", test_policy_move_label },
  { nullptr, nullptr }
};
