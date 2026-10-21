#pragma once

#include "Hashing.h"
#include "CsfCodebook.h"
#include "ConstructUtils.h"
#include <vector>

namespace caramel {

struct BucketQueryInfo {
  const uint64_t *data;
  uint32_t num_variables;
  uint32_t seed;
};

// Decodes one cell's value for a key whose signature is already known: the
// bit-slice + canonical-decode tail shared by every index type.
template <typename T>
inline T __attribute__((always_inline))
decodeBucketColumn(const __uint128_t &signature, const BucketQueryInfo &info,
                   uint32_t max_codelength,
                   const std::vector<uint32_t> &code_length_counts,
                   const std::vector<T> &ordered_symbols) {
  uint64_t e[3];
  signatureToEquation(signature, info.seed, info.num_variables, e);

  const uint64_t *arr = info.data;
  const int l = 64 - static_cast<int>(max_codelength);
  auto getbits = [arr, l](uint32_t pos) __attribute__((always_inline)) {
    const uint64_t w = pos / 64;
    const int b = pos % 64;
    if (b <= l)
      return arr[w] << b >> l;
    return arr[w] << b >> l | arr[w + 1] >> (128 - (-l + 64) - b);
  };

  uint64_t encoded_value = getbits(e[0]) ^ getbits(e[1]) ^ getbits(e[2]);

  return canonicalDecodeFromNumber<T>(encoded_value, code_length_counts,
                                      ordered_symbols, max_codelength);
}

} // namespace caramel
