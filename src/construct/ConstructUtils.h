#pragma once

#include "SpookyHash.h"
#include <algorithm>
#include <array>
#include <functional>
#include <vector>

namespace caramel {

void inline signatureToEquation(const Uint128Signature &signature,
                                const uint64_t seed, uint64_t num_variables,
                                uint64_t *e) {
  uint64_t hash[4];
  SpookyHash::SpookyShortRehash(signature, seed, hash);
  const int shift = __builtin_clzll(num_variables);
  const uint64_t mask = (UINT64_C(1) << shift) - 1;
  e[0] = ((hash[0] & mask) * num_variables) >> shift;
  e[1] = ((hash[1] & mask) * num_variables) >> shift;
  e[2] = ((hash[2] & mask) * num_variables) >> shift;
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