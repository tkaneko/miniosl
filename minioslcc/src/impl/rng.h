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
    typedef std::default_random_engine rng_t;
    typedef std::array<rng_t, available_instances> rng_array_t;
    extern rng_array_t rngs;

    rng_t make_rng();
    rng_array_t make_rng_array();
  }
  using rng::rngs;
  using rng::rng_t;
  using rng::rng_array_t;
}

#endif
// MINIOSL_RNG_H
