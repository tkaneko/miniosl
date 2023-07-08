#include <thread>

namespace osl {
  inline void run_range_parallel(int N, auto f) {
#ifndef ENABLE_RANGE_PARALLEL
    f(0, N);
#else
    if (N < 64) { f(0, N); }
    else {
      int c0 = N / 4, c1 = N / 2, c2 = N - c0;
      std::thread t1(f, 0, c0), t2(f, c0, c1), t3(f, c1, c2), t4(f, c2, N);
      t1.join(); t2.join(); t3.join(); t4.join();
    }
#endif    
  }
}
