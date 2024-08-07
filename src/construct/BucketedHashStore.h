#pragma once

#include "SpookyHash.h"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace caramel {

inline uint32_t getBucketID(const Uint128Signature &signature,
                            uint32_t num_buckets) {
  // Use first 64 bits of the signature to identify the segment
  uint64_t bucket_hash = signature.first;
  // This outputs a uniform integer from [0, self._num_buckets]
  uint32_t bucket_id =
      ((__uint128_t)(bucket_hash >> 1) * (__uint128_t)(num_buckets << 1)) >> 64;
  // TODO(any) should we rewrite this function to not use __uint128_t due to
  // platform incompatabilities?
  return bucket_id;
}

Uint128Signature hashKey(const std::string &key, uint64_t seed);

template <typename T>
std::tuple<std::vector<std::vector<Uint128Signature>>,
           std::vector<std::vector<T>>, uint64_t>
construct(const std::vector<std::string> &keys, const std::vector<T> &values,
          uint32_t num_buckets, uint64_t seed) {
  if (keys.size() != values.size()) {
    throw std::invalid_argument("Keys and values must match sizes.");
  }

  std::vector<std::vector<Uint128Signature>> key_buckets(num_buckets);
  std::vector<std::vector<T>> value_buckets(num_buckets);

  for (uint32_t i = 0; i < keys.size(); i++) {
    Uint128Signature signature = hashKey(keys[i], seed);
    uint32_t bucket_id = getBucketID(signature, num_buckets);
    if (std::find(key_buckets[bucket_id].begin(), key_buckets[bucket_id].end(),
                  signature) != key_buckets[bucket_id].end()) {
      throw std::runtime_error("Detected a key collision under 128-bit hash. "
                               "Likely due to a duplicate key.");
    }
    key_buckets[bucket_id].push_back(signature);
    value_buckets[bucket_id].push_back(values[i]);
  }

  return {key_buckets, value_buckets, seed};
}

template <typename T>
std::tuple<std::vector<std::vector<Uint128Signature>>,
           std::vector<std::vector<T>>, uint64_t>
partitionToBuckets(const std::vector<std::string> &keys,
                   const std::vector<T> &values, uint32_t bucket_size = 1000,
                   uint32_t num_attempts = 3) {
  if (keys.size() != values.size()) {
    throw std::invalid_argument("Keys and values must match sizes.");
  }
  uint32_t size = keys.size();
  uint32_t num_buckets = 1 + (size / bucket_size); // TODO check this division?

  for (uint64_t seed = 0; seed < num_attempts; seed++) {
    try {
      return construct<T>(keys, values, num_buckets, seed);
    } catch (const std::exception &e) {
      if (seed == num_attempts - 1) {
        throw;
      }
      continue;
    }
  }

  throw std::invalid_argument("Fatal error: should never reach here.");
}

} // namespace caramel