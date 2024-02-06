#pragma once

#include "SpookyHash.h"
#include <algorithm>
#include <array>
#include <functional>
#include <vector>

namespace caramel {

inline uint32_t numberOfLeadingZeros(uint32_t value) {
  // Handle zero explicitly, as __builtin_clz(0) is undefined
  if (value == 0) {
    return 32;
  }
  return __builtin_clz(value);
}

inline std::vector<uint32_t>
signatureToEquation(const Uint128Signature &key_signature, uint32_t seed,
                    uint32_t num_variables) {
  // TODO lets write a custom hash function that generates three 64 bit hashes
  // instead of hashing 3 times
  uint32_t hash0 = SpookyHash::Hash64(&key_signature.first,
                                      /* length = */ 8, seed++);
  uint32_t hash1 = SpookyHash::Hash64(&key_signature.first,
                                      /* length = */ 8, seed++);
  uint32_t hash2 = SpookyHash::Hash64(&key_signature.first,
                                      /* length = */ 8, seed++);
  uint32_t shift = numberOfLeadingZeros(num_variables);
  unsigned long long mask = (1ULL << shift) - 1;
  std::vector<uint32_t> start_var_locations(3);
  start_var_locations[0] = (int)(((hash0 & mask) * num_variables) >> shift);
  start_var_locations[1] = (int)(((hash1 & mask) * num_variables) >> shift);
  start_var_locations[2] = (int)(((hash2 & mask) * num_variables) >> shift);
  return start_var_locations;
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