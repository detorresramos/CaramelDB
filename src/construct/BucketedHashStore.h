#pragma once

#include "SpookyHash.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace caramel {

inline uint32_t getBucketID(const __uint128_t &signature,
                            uint32_t num_buckets) {
  // Use first 64 bits of the signature to identify the segment
  uint64_t bucket_hash = static_cast<uint64_t>(signature);
  // This outputs a uniform integer from [0, self._num_buckets]
  uint32_t bucket_id =
      ((__uint128_t)(bucket_hash >> 1) * (__uint128_t)(num_buckets << 1)) >> 64;
  // TODO(any) should we rewrite this function to not use __uint128_t due to
  // platform incompatabilities?
  return bucket_id;
}

inline __uint128_t hashKey(const std::string &key, uint64_t seed) {
  const void *msgPtr = static_cast<const void *>(key.data());
  size_t length = key.size();
  uint64_t hash1 = seed;
  uint64_t hash2 = seed;
  SpookyHash::Hash128(msgPtr, length, &hash1, &hash2);
  return (static_cast<__uint128_t>(hash1) << 64) | hash2;
}

template <typename T> struct BucketedHashStore {
  std::vector<std::vector<__uint128_t>> key_buckets;
  std::vector<std::vector<T>> value_buckets;
  uint64_t seed;
  uint64_t num_buckets;
};

template <typename T>
BucketedHashStore<T> construct(const std::vector<std::string> &keys,
                               const std::vector<T> &values,
                               uint32_t num_buckets, uint64_t seed,
                               uint64_t approximate_bucket_size) {
  if (keys.size() != values.size()) {
    throw std::invalid_argument("Keys and values must match sizes.");
  }

  std::vector<std::vector<__uint128_t>> key_buckets(num_buckets);
  std::vector<std::vector<T>> value_buckets(num_buckets);

  for (size_t i = 0; i < num_buckets; i++) {
    key_buckets[i].reserve(approximate_bucket_size);
    value_buckets[i].reserve(approximate_bucket_size);
  }

  for (size_t i = 0; i < keys.size(); i++) {
    __uint128_t signature = hashKey(keys[i], seed);
    uint32_t bucket_id = getBucketID(signature, num_buckets);
    key_buckets[bucket_id].push_back(signature);
    value_buckets[bucket_id].push_back(values[i]);
  }

  std::exception_ptr exception = nullptr;
#pragma omp parallel for default(none)                                         \
    shared(num_buckets, key_buckets, value_buckets, exception)
  for (size_t i = 0; i < num_buckets; i++) {
    const auto &bucket = key_buckets.at(i);
    std::unordered_set<__uint128_t> uniques(bucket.begin(), bucket.end());
    if (uniques.size() != bucket.size()) {
#pragma omp critical
      {
        exception = std::make_exception_ptr(
            std::runtime_error("Detected a key collision under 128-bit hash. "
                               "Likely due to a duplicate key."));
      }
    }
  }

  if (exception) {
    std::rethrow_exception(exception);
  }

  return {key_buckets, value_buckets, seed, key_buckets.size()};
}

template <typename T>
BucketedHashStore<T> partitionToBuckets(const std::vector<std::string> &keys,
                                        const std::vector<T> &values,
                                        uint64_t num_buckets,
                                        uint32_t num_attempts = 3) {
  if (keys.size() != values.size()) {
    throw std::invalid_argument("Keys and values must match sizes.");
  }
  uint64_t approximate_bucket_size = keys.size() / num_buckets + 1;

  for (uint64_t seed = 0; seed < num_attempts; seed++) {
    try {
      return construct<T>(keys, values, num_buckets, seed,
                          approximate_bucket_size);
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