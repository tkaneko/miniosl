#include "filepath.h"
#include "feature.h"
#include "game.h"
#include "impl/bitpack.h"

using namespace osl;

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
      TEST_ASSERT(state.isAcceptable(move));
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
        if (ml::decode_move_label(code, state) != m) {
          std::cerr << m << " != " << ml::decode_move_label(code, state)
                    << '\n' << state ;
        }
        TEST_ASSERT(ml::decode_move_label(code, state) == m);
      }

      state.makeMove(move);
    }
  }
}

void test_make_move_unsafe() {
  const auto& data = test_record_set();
  int count = 0;
  MoveVector legal_moves;
  for (const auto& record: data.records) {
    if (++count > limit)
      break;
    auto state(record.initial_state);
    BaseState base {state};
    for (auto move: record.moves) {
      state.makeMove(move);
      base.make_move_unsafe(move);
      TEST_CHECK(state == base);
      TEST_CHECK(state.check_internal_consistency());
      TEST_CHECK(base.check_internal_consistency());
    }
  }
}

void test_export_features() {
  std::vector<nn_input_element> work(osl::ml::input_unit);
  
  const auto& data = test_record_set();
  int count = 0;
  for (const auto& record: data.records) {
    if (++count > limit)
      break;
    
    GameManager mgr;
    
    for (auto move: record.moves) {
      std::fill(work.begin(), work.end(), 0);
      mgr.export_heuristic_feature(&work[0]);
      auto ret = mgr.make_move(move);
      if (ret != InGame)
        break;
    }
  }
}


TEST_LIST = {
  { "export_features", test_export_features },
  { "japanese", test_japanese },
  { "hash", test_hash },
  { "policy_move_label", test_policy_move_label },
  { "make_move_unsafe", test_make_move_unsafe },
  { nullptr, nullptr }
};
