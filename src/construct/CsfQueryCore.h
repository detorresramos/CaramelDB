#pragma once

#include "CsfCodebook.h"
#include "Hashing.h"
#include "Ribbon.h"
#include <stdexcept>
#include <vector>

namespace caramel {

struct BucketQueryInfo {
  const uint64_t *data;
  uint32_t num_variables; // band start positions
  uint32_t seed;
};

// Canonical Huffman decode of the next code from a bit source; adds the code
// length to *length when given.
template <typename T, typename Bits>
inline T __attribute__((always_inline))
canonicalDecodeNext(Bits &bits, const std::vector<uint32_t> &code_length_counts,
                    const std::vector<T> &symbols, uint32_t *length = nullptr) {
  int code = 0;
  int first = 0;
  int index = 0;
  for (uint32_t i = 1; i < code_length_counts.size(); i++) {
    code |= bits.next();
    const int count = code_length_counts[i];
    if (code - count < first) {
      if (length) {
        *length += i;
      }
      return symbols[index + (code - first)];
    }
    index += count;
    first += count;
    first <<= 1;
    code <<= 1;
  }
  throw std::invalid_argument("Invalid Code Passed");
}

// Decodes one cell's value for a key whose hash is already known: the decode
// shared by every index type.
template <typename T>
inline T __attribute__((always_inline))
decodeBucketColumn(const RibbonHash &hash, const BucketQueryInfo &info,
                   const std::vector<uint32_t> &code_length_counts,
                   const std::vector<T> &ordered_symbols) {
  RibbonReader bits(info.data, hash, info.seed, info.num_variables);
  return canonicalDecodeNext<T>(bits, code_length_counts, ordered_symbols);
}

} // namespace caramel
