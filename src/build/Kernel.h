#pragma once

#include <algorithm>
#include <cmath>
#include <numeric>
#include <optional>
#include <src/construct/Ribbon.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace caramel::build {

// Equations per cell. Measured on 10M-key builds: smaller cells cost disk
// (the band tail and the per-cell metadata) and solve time; larger cells
// spread a key's bands over more cache lines and near the solvability cliff
// (at 8000 equations, 3% overhead stops solving). A query reads one band per
// code bit, so short codes touch few lines in any cell, while long codes touch
// every line of theirs: cells shrink as codes grow (zipf: 73 -> 66 ns). Below
// 1500 equations the metadata outgrows the latency gain.
static constexpr uint64_t MAX_EQUATIONS_PER_BUCKET = 3000;
static constexpr uint64_t MIN_EQUATIONS_PER_BUCKET = 1500;
static constexpr double CELL_EQUATIONS_TIMES_CODE_BITS = 12000;

inline double targetEquationsPerBucket(uint64_t num_equations, uint64_t num_keys) {
  const double code_bits = static_cast<double>(num_equations) / std::max<uint64_t>(num_keys, 1);
  return std::clamp(CELL_EQUATIONS_TIMES_CODE_BITS / std::max(code_bits, 1.0),
                    double{MIN_EQUATIONS_PER_BUCKET}, double{MAX_EQUATIONS_PER_BUCKET});
}

// The smallest starts >= s whose stride is coprime to it, so that a key with
// a long code stream visits every band start before reusing one.
inline uint32_t coprimeStarts(uint32_t s) {
  while (s > 1 && std::gcd(ribbonStride(s), s) != 1) {
    s++;
  }
  return s;
}

// Band start positions of a cell with `num_equations` equations. Its solution
// has starts + RIBBON_WIDTH - 1 bits.
inline uint32_t ribbonStarts(uint64_t num_equations,
                             double overhead = RIBBON_OVERHEAD) {
  const auto bits = static_cast<uint64_t>(
      std::ceil(static_cast<double>(num_equations) * overhead));
  return coprimeStarts(static_cast<uint32_t>(
      std::max<uint64_t>(1, bits > RIBBON_WIDTH ? bits - (RIBBON_WIDTH - 1) : 1)));
}

// Buckets for a column with `num_keys` keys and `num_equations` code bits.
inline uint64_t columnBucketCount(uint64_t num_equations, uint64_t num_keys) {
  const double target = targetEquationsPerBucket(num_equations, num_keys);
  return std::clamp<uint64_t>(static_cast<uint64_t>(num_equations / target),
                              num_keys / 10000 + 1, num_keys / 16 + 1);
}

// Packed rows carry many codes each, so they get no upper clamp.
inline uint64_t packedBucketCount(uint64_t num_equations, uint64_t num_keys) {
  return std::max(num_equations / MAX_EQUATIONS_PER_BUCKET, num_keys / 10000 + 1);
}

namespace detail {

inline bool hasDuplicate(const __uint128_t *sigs, size_t n) {
  std::vector<__uint128_t> sorted(sigs, sigs + n);
  std::sort(sorted.begin(), sorted.end());
  return std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end();
}

// Gaussian elimination of one banded row into the pivot rows: false if the
// row reduces to 0 = 1.
inline bool ribbonInsert(uint64_t *coef, uint8_t *rhs, uint32_t pos,
                         uint64_t c, uint8_t r) {
  while (true) {
    if (!coef[pos]) {
      coef[pos] = c;
      rhs[pos] = r;
      return true;
    }
    c ^= coef[pos];
    r ^= rhs[pos];
    if (!c) {
      return r == 0;
    }
    const int shift = __builtin_ctzll(c);
    c >>= shift;
    pos += shift;
  }
}

// Back substitution, last pivot first; free variables are 0.
inline void ribbonBackSubstitute(const uint64_t *coef, const uint8_t *rhs,
                                 uint64_t bits, uint64_t *out) {
  uint64_t window = 0; // bit t is Z[i + 1 + t]
  for (uint64_t i = bits; i-- > 0;) {
    uint64_t z = 0;
    if (coef[i]) {
      z = rhs[i] ^ __builtin_parityll((coef[i] >> 1) & window);
    }
    window = (window << 1) | z;
    out[i / 64] |= z << (i % 64);
  }
}

} // namespace detail

// Solves one cell: n keys with signatures sigs, where emit(i, add) calls
// add(j, bit) for code bits j = 0, 1, ... of key i. Writes cellWords(starts)
// words to out, which must be zero, and returns the seed that solved it, or
// nothing if no seed below RIBBON_SEEDS does (the caller then grows the cell).
// A key given twice with different values makes every seed fail, so a cell
// that fails once is checked for duplicate signatures; a key given twice with
// the same value solves and is harmless.
static constexpr uint32_t RIBBON_SEEDS = 64;

template <typename Emit>
std::optional<uint8_t> solveCell(const __uint128_t *sigs, size_t n,
                                 uint32_t starts, uint64_t *out, Emit &&emit) {
  const uint64_t bits = uint64_t{starts} + RIBBON_WIDTH - 1;
  thread_local std::vector<uint64_t> coef;
  thread_local std::vector<uint8_t> rhs;
  for (uint32_t seed = 0; seed < RIBBON_SEEDS; seed++) {
    coef.assign(bits, 0);
    rhs.assign(bits, 0);
    bool solvable = true;
    for (size_t i = 0; i < n && solvable; i++) {
      const RibbonKey key(RibbonHash(sigs[i]), seed, starts);
      uint32_t next_j = 0;
      uint32_t pos = key.start;
      emit(i, [&](uint32_t j, uint64_t bit) {
        pos = j == next_j ? pos : key.position(j);
        solvable = solvable && detail::ribbonInsert(coef.data(), rhs.data(), pos,
                                                    key.coefficient(j), bit);
        next_j = j + 1;
        pos = key.next(pos);
      });
    }
    if (solvable) {
      detail::ribbonBackSubstitute(coef.data(), rhs.data(), bits, out);
      return static_cast<uint8_t>(seed);
    }
    if (seed == 0 && detail::hasDuplicate(sigs, n)) {
      throw std::runtime_error("Detected a key collision under 128-bit hash. "
                               "Likely due to a duplicate key.");
    }
  }
  return std::nullopt;
}

// Band starts for a cell that did not solve with `starts`.
inline uint32_t grownStarts(uint32_t starts) {
  return coprimeStarts(starts + std::max<uint32_t>(64, starts / 20));
}

} // namespace caramel::build
