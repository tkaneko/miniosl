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
    int prev=0;
    for (int i=0; i<range_parallel_threads; ++i) {
      int c = (i+1) < range_parallel_threads
        ? N*(i+1) / range_parallel_threads
        : N;
      workers.emplace_back(f, prev, c);
      prev = c;
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
    int prev=0;
    for (int i=0; i<range_parallel_threads; ++i) {
      int c = (i+1) < range_parallel_threads
        ? N*(i+1) / range_parallel_threads
        : N;
      workers.emplace_back(f, prev, c, TID(i));
      prev = c;
    }
    for (auto& w: workers)
      w.join();
  }
}
