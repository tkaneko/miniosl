#ifndef MINIOSL_RNG_H
#define MINIOSL_RNG_H

#include <random>

namespace osl {
  extern std::array<std::default_random_engine,4> rngs;
}

#endif
// MINIOSL_RNG_H
