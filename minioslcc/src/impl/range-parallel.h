#include <thread>

namespace osl {
  constexpr int range_parallel_threads = 4;
  
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

  inline void run_range_parallel_tid(int N, auto f) {
#ifndef ENABLE_RANGE_PARALLEL
    f(0, N, TID_ZERO);
#else
    if (N < 64) { f(0, N, TID_ZERO); }
    else {
      int c0 = N / 4, c1 = N / 2, c2 = N - c0;
      // the last parameter is for thread id
      std::thread t1(f, 0, c0, TID(0)), t2(f, c0, c1, TID(1)), t3(f, c1, c2, TID(2)), t4(f, c2, N, TID(3));
      t1.join(); t2.join(); t3.join(); t4.join();
    }
#endif    
  }
}
