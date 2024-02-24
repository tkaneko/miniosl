#include "impl/rng.h"

osl::rng::rng_t osl::rng::make_rng() {
    static const auto env = std::getenv("MINIOSL_DETERMINISTIC");
    static int cnt = 0;
    static std::random_device rdev;
    return std::default_random_engine(env ? (cnt++) : rdev());
}

osl::rng::rng_array_t osl::rng::make_rng_array() {
  return {
    make_rng(), make_rng(), make_rng(), make_rng(),  make_rng(), make_rng(), make_rng(), make_rng(),
    make_rng(), make_rng(), make_rng(), make_rng(),  make_rng(), make_rng(), make_rng(), make_rng(),
  };
}

namespace osl {
  rng::rng_array_t rng::rngs = rng::make_rng_array();
}
