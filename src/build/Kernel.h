#pragma once

#include <algorithm>
#include <cmath>
#include <src/BitArray.h>
#include <src/Modulo2System.h>
#include <src/construct/ConstructUtils.h>
#include <src/solve/Solve.h>
#include <stdexcept>
#include <string>
#include <utility>

namespace caramel::build {

// Variables per equation. Makes a 3-hash system solvable with very high
// probability under lazy Gaussian elimination; dropping lazy elimination
// would need ~1.23 (12% more memory) for faster construction.
static constexpr float DELTA = 1.089f;

static constexpr uint64_t TARGET_EQUATIONS_PER_BUCKET = 3500;

inline uint64_t numVariables(uint64_t num_equations) {
  return static_cast<uint64_t>(
      std::ceil(static_cast<double>(num_equations) * DELTA));
}

// Buckets for a column with `num_keys` keys and `num_equations` code bits.
inline uint64_t columnBucketCount(uint64_t num_equations, uint64_t num_keys) {
  return std::clamp(num_equations / TARGET_EQUATIONS_PER_BUCKET,
                    num_keys / 1000 + 1, num_keys / 100 + 1);
}

// Packed rows carry many codes each, so they get no upper clamp.
inline uint64_t packedBucketCount(uint64_t num_equations, uint64_t num_keys) {
  return std::max(num_equations / TARGET_EQUATIONS_PER_BUCKET,
                  num_keys / 1000 + 1);
}

// Builds and solves the system of one cell: n keys with signatures sigs,
// where emit(i, add) calls add(offset, bit) for each code bit of key i. The
// solution has numVariables(num_equations) + pad bits; pad lets a query read
// a full code window starting at any variable. Retries solve seeds until the
// system is solvable.
template <typename Emit>
std::pair<BitArrayPtr, uint32_t>
solveCell(const __uint128_t *sigs, size_t n, uint64_t num_equations,
          uint32_t pad, Emit &&emit) {
  constexpr uint32_t MAX_ATTEMPTS = 128;
  const uint64_t num_vars = numVariables(num_equations);
  for (uint32_t seed = 0; seed < MAX_ATTEMPTS; seed++) {
    auto system = SparseSystem::make(num_equations, num_vars + pad);
    uint64_t start[3];
    for (size_t i = 0; i < n; i++) {
      signatureToEquation(sigs[i], seed, num_vars, start);
      emit(i, [&](uint32_t offset, uint64_t bit) {
        system->addEquation(start, offset, bit);
      });
    }
    try {
      return {solveModulo2System(system), seed};
    } catch (const UnsolvableSystemException &) {
    }
  }
  throw std::runtime_error("Tried to solve system " +
                           std::to_string(MAX_ATTEMPTS) +
                           " times with no success.");
}

} // namespace caramel::build
