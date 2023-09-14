#include "rng.h"
#include <thread>
#include <algorithm>
#include <vector>

namespace osl {
#ifndef ENABLE_RANGE_PARALLEL
  const int range_parallel_threads = 1;
#else
  extern const int range_parallel_threads;
#endif
  
  inline void run_range_parallel(int N, auto f) {
    if (range_parallel_threads < 2 || N < 64) {
      f(0, N);
      return;
    }
    std::vector<std::thread> workers;
    workers.reserve(range_parallel_threads);
    constexpr int algn = 16;
    int size = (N+algn*range_parallel_threads-1)/algn/range_parallel_threads * algn;
    for (int i=0; i<range_parallel_threads; ++i) {
      int l = std::min(size*i, N), r = std::min(l+size, N);
      workers.emplace_back(f, l, r);
    }
    for (auto& w: workers)
      w.join();
  }

  inline void run_range_parallel_tid(int N, auto f) {
    if (range_parallel_threads < 2 || N < 64) {
      f(0, N, TID_ZERO);
      return;
    }
    std::vector<std::thread> workers;
    workers.reserve(range_parallel_threads);
    constexpr int algn = 16;
    int size = (N+algn*range_parallel_threads-1)/algn/range_parallel_threads * algn;
    for (int i=0; i<range_parallel_threads; ++i) {
      int l = std::min(size*i, N), r = std::min(l+size, N);
      workers.emplace_back(f, l, r, TID(i));
    }
    for (auto& w: workers)
      w.join();
  }
}
