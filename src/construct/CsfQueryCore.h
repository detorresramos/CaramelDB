#pragma once

#include "BucketedHashStore.h"
#include "CsfCodebook.h"
#include "ConstructUtils.h"
#include <vector>

namespace caramel {

struct BucketQueryInfo {
  const uint64_t *data;
  uint32_t num_variables;
  uint32_t seed;
};

template <typename T>
T queryCsfCore(const char *data, size_t length, uint32_t hash_store_seed,
               const std::vector<BucketQueryInfo> &bucket_info,
               uint32_t num_buckets, uint32_t max_codelength,
               const HuffmanLookupTable<T> &lookup_table,
               const std::vector<uint32_t> &code_length_counts,
               const std::vector<T> &ordered_symbols) {
  __uint128_t signature = hashKey(data, length, hash_store_seed);

  uint32_t bucket_id = getBucketID(signature, num_buckets);

  const auto &info = bucket_info[bucket_id];

  uint64_t e[3];
  signatureToEquation(signature, info.seed, info.num_variables, e);

  const uint64_t *arr = info.data;
  const int l = 64 - max_codelength;
  auto getbits = [arr, l](uint32_t pos) __attribute__((always_inline)) {
    const uint64_t w = pos / 64;
    const int b = pos % 64;
    if (b <= l)
      return arr[w] << b >> l;
    return arr[w] << b >> l | arr[w + 1] >> (128 - (-l + 64) - b);
  };

  uint64_t encoded_value = getbits(e[0]) ^ getbits(e[1]) ^ getbits(e[2]);

  if (lookup_table.symbols_by_code.empty()) {
    return canonicalDecodeFromNumber<T>(encoded_value, code_length_counts,
                                        ordered_symbols, max_codelength);
  }
  return lookup_table.decode(encoded_value);
}

} // namespace caramel
