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

// Reads the max_codelength-bit window at each of the three variable positions
// and XORs them. Split out from decodeBucketColumn so callers that compute the
// positions ahead of time (to prefetch them) can reuse the slice.
inline uint64_t __attribute__((always_inline))
gatherEncodedValue(const uint64_t *arr, const uint64_t *e,
                   uint32_t max_codelength) {
  const int l = 64 - static_cast<int>(max_codelength);
  auto getbits = [arr, l](uint32_t pos) __attribute__((always_inline)) {
    const uint64_t w = pos / 64;
    const int b = pos % 64;
    if (b <= l)
      return arr[w] << b >> l;
    return arr[w] << b >> l | arr[w + 1] >> (128 - (-l + 64) - b);
  };
  return getbits(e[0]) ^ getbits(e[1]) ^ getbits(e[2]);
}

// Decodes one bucket's value for a key whose signature is already known. The
// single source of the bit-slice + canonical-decode tail, shared by
// queryCsfCore (single Csf) and MultisetCsf/RaggedMultisetCsf group queries.
template <typename T>
inline T __attribute__((always_inline))
decodeBucketColumn(const __uint128_t &signature, const BucketQueryInfo &info,
                   uint32_t max_codelength,
                   const std::vector<uint32_t> &code_length_counts,
                   const std::vector<T> &ordered_symbols) {
  uint64_t e[3];
  signatureToEquation(signature, info.seed, info.num_variables, e);

  uint64_t encoded_value = gatherEncodedValue(info.data, e, max_codelength);

  return canonicalDecodeFromNumber<T>(encoded_value, code_length_counts,
                                      ordered_symbols, max_codelength);
}

template <typename T>
T queryCsfCore(const char *data, size_t length, uint32_t hash_store_seed,
               const std::vector<BucketQueryInfo> &bucket_info,
               uint32_t num_buckets, uint32_t max_codelength,
               const std::vector<uint32_t> &code_length_counts,
               const std::vector<T> &ordered_symbols) {
  __uint128_t signature = hashKey(data, length, hash_store_seed);
  uint32_t bucket_id = getBucketID(signature, num_buckets);
  return decodeBucketColumn<T>(signature, bucket_info[bucket_id], max_codelength,
                               code_length_counts, ordered_symbols);
}

} // namespace caramel
