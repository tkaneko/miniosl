#include "acutest.h"

#include "state.h"
#include "record.h"

#include <filesystem>
#include <iostream>
#include <fstream>

inline auto make_path() {
  return std::filesystem::path("csa");
}

#ifdef NDEBUG
const int limit = 65536;
#else
const int limit = 2000;
#endif

const auto& test_record_set() {
  static auto data = osl::RecordSet::from_path("csa", limit);
  return data;
}

#define TEST_CHECK_EQUAL(a,b) TEST_CHECK((a) == (b))
#define TEST_ASSERT_EQUAL(a,b) TEST_ASSERT((a) == (b))

