#include "acutest.h"
#include "state.h"
#include "record.h"
#include "feature.h"
#include "game.h"
#include "impl/more.h"
#include "impl/checkmate.h"
#include "impl/bitpack.h"
#include <iostream>
#include <bitset>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <random>
#include <unordered_map>

#define TEST_CHECK_EQUAL(a,b) TEST_CHECK((a) == (b))
#define TEST_ASSERT_EQUAL(a,b) TEST_ASSERT((a) == (b))

using namespace osl;
using namespace std::string_literals;

void test_state816k() {
  {
    BaseState state(Shogi816K, 0);
    TEST_ASSERT(state.check_internal_consistency());
  }
  for (int i=0; i<1024; ++i) {
    BaseState state(Shogi816K, -1);
    TEST_ASSERT(state.check_internal_consistency());
  }
  std::unordered_map<std::string,int> table;
  for (int i=0; i<1632960; ++i) {
    BaseState state(Shogi816K, i);
    TEST_ASSERT(state.check_internal_consistency());
    auto s = to_usi(state);
    // std::cerr << i << ' ' << s << '\n';
    
    TEST_ASSERT(table.find(s) == table.end());
    table[s] = i;

    auto id = state.shogi816kID();
    TEST_ASSERT(id.has_value());
    TEST_ASSERT(id.value() == i);
  }
}

void test_record() {
  {
    MiniRecord record;
    int id = 7777;
    BaseState state(Shogi816K, id);
    TEST_ASSERT(BaseState(HIRATE) != state);
    record.set_initial_state(state, id);
    SubRecord sub(record);
    TEST_ASSERT(sub.initial_state() == state);
  }
}

void test_game() {
  {
    int id = 7778;
    GameManager game(id);
    
  }
}

TEST_LIST = {
  { "state816k", test_state816k },
  { "record", test_record },
  { "game", test_game },
  { nullptr, nullptr }
};
