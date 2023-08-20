#ifndef MINIOSL_RNG_H
#define MINIOSL_RNG_H

#include <random>
#include <array>

namespace osl {
  /** thread local random number generators.
   *
   * initialized by the standard random device unless `std::getenv("MINIOSL_DETERMINISTIC")`
   */
  namespace rng {
    constexpr int available_instances = 16;
    extern std::array<std::default_random_engine, available_instances> rngs;
  }
  using rng::rngs;
}

#endif
// MINIOSL_RNG_H
