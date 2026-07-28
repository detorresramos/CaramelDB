#pragma once

#include "SpookyHash.h"
#include <array>
#include <functional>

namespace caramel {

// Folded 128-bit product (wyhash's "mum" primitive): one multiply mixing two
// 64-bit words into one.
inline uint64_t mumMix(uint64_t a, uint64_t b) {
  __uint128_t product = static_cast<__uint128_t>(a) * b;
  return static_cast<uint64_t>(product) ^ static_cast<uint64_t>(product >> 64);
}

// Derives the three equation positions for one (signature, seed) pair. The
// signature is already a full-strength 128-bit SpookyHash of the key, so the
// per-column derivation only needs to decorrelate columns/seeds, not build
// avalanche from scratch: three independent multiply-folds do that at a
// fraction of the cost of SpookyHash's 12-round ShortMix. A multiset query
// runs one of these per column, which made ShortMix the largest single compute
// term in the fan-out. Construction uses the identical derivation, and the
// per-bucket seed retry (bump seed, re-derive) remains the fallback if a
// bucket's hypergraph is unpeelable.
void inline signatureToEquation(const __uint128_t &signature,
                                const uint64_t seed, uint64_t num_variables,
                                uint64_t *e) {
  if (num_variables == 0) {
    // An empty bucket has no variables; __builtin_clzll(0) is undefined, so
    // short-circuit. The caller decodes nothing from a zero-variable range.
    e[0] = e[1] = e[2] = 0;
    return;
  }
  const uint64_t lo = static_cast<uint64_t>(signature);
  const uint64_t hi = static_cast<uint64_t>(signature >> 64);
  const uint64_t s =
      seed * UINT64_C(0x9E3779B97F4A7C15) + UINT64_C(0xD1B54A32D192ED03);
  const uint64_t a = lo ^ s;
  const uint64_t b = hi + s;
  const uint64_t h0 = mumMix(a, b ^ UINT64_C(0xA0761D6478BD642F));
  const uint64_t h1 = mumMix(a ^ UINT64_C(0xE7037ED1A0B428DB), b);
  const uint64_t h2 = mumMix(a + UINT64_C(0x8EBC6AF09C88C6E3),
                             b ^ UINT64_C(0x589965CC75374CC3));
  const int shift = __builtin_clzll(num_variables);
  const uint64_t mask = (UINT64_C(1) << shift) - 1;
  e[0] = ((h0 & mask) * num_variables) >> shift;
  e[1] = ((h1 & mask) * num_variables) >> shift;
  e[2] = ((h2 & mask) * num_variables) >> shift;
}

} // namespace caramel

namespace std {
template <std::size_t N> struct hash<std::array<char, N>> {
  size_t operator()(const std::array<char, N> &arr) const {
    size_t hash = 0;
    for (const char &c : arr) {
      hash = hash * 31 + c;
    }
    return hash;
  }
};

} // namespace std