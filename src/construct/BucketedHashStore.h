#pragma once

#include "SpookyHash.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#if defined(__GNUC__) && !defined(__clang__)
namespace std {
template <> struct hash<__uint128_t> {
  size_t operator()(__uint128_t v) const noexcept {
    auto lo = static_cast<uint64_t>(v);
    auto hi = static_cast<uint64_t>(v >> 64);
    return hash<uint64_t>{}(lo) ^ (hash<uint64_t>{}(hi) * 0x9e3779b97f4a7c15ULL + 0x9e3779b9 + (lo << 6) + (lo >> 2));
  }
};
} // namespace std
#endif

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

inline __uint128_t hashKey(const char *data, size_t length, uint64_t seed) {
  uint64_t hash1 = seed;
  uint64_t hash2 = seed;
  SpookyHash::Hash128(static_cast<const void *>(data), length, &hash1, &hash2);
  return (static_cast<__uint128_t>(hash1) << 64) | hash2;
}

inline __uint128_t hashKey(const std::string &key, uint64_t seed) {
  return hashKey(key.data(), key.size(), seed);
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

// A key partition computed once and reused to bucket many columns' values.
// All columns of a multiset group share the same keys + seed, so hashing the
// keys per column (the dominant build cost) is wasteful; hash once here and
// scatter each column's values with scatterValues().
struct KeyPartition {
  std::vector<std::vector<__uint128_t>> key_buckets;  // signatures, grouped
  std::vector<uint32_t> bucket_of_key;                // bucket id per key (input order)
  uint64_t seed = 0;
  uint32_t num_buckets = 0;
};

inline KeyPartition partitionKeys(const std::vector<std::string> &keys,
                                  uint32_t num_buckets,
                                  uint32_t num_attempts = 3) {
  uint64_t approximate_bucket_size = keys.size() / num_buckets + 1;
  for (uint64_t seed = 0; seed < num_attempts; seed++) {
    KeyPartition part;
    part.seed = seed;
    part.num_buckets = num_buckets;
    part.key_buckets.assign(num_buckets, {});
    part.bucket_of_key.resize(keys.size());
    for (uint32_t b = 0; b < num_buckets; b++) {
      part.key_buckets[b].reserve(approximate_bucket_size);
    }

    for (size_t i = 0; i < keys.size(); i++) {
      __uint128_t signature = hashKey(keys[i], seed);
      uint32_t bucket_id = getBucketID(signature, num_buckets);
      part.key_buckets[bucket_id].push_back(signature);
      part.bucket_of_key[i] = bucket_id;
    }

    bool collision = false;
#pragma omp parallel for default(none) shared(num_buckets, part, collision)
    for (size_t b = 0; b < num_buckets; b++) {
      const auto &bucket = part.key_buckets[b];
      std::unordered_set<__uint128_t> uniques(bucket.begin(), bucket.end());
      if (uniques.size() != bucket.size()) {
#pragma omp critical
        collision = true;
      }
    }
    if (!collision) {
      return part;
    }
    if (seed == num_attempts - 1) {
      throw std::runtime_error("Detected a key collision under 128-bit hash. "
                               "Likely due to a duplicate key.");
    }
  }
  throw std::invalid_argument("Fatal error: should never reach here.");
}

// Buckets one column's values under a precomputed key partition (no hashing).
template <typename T>
std::vector<std::vector<T>> scatterValues(const std::vector<T> &values,
                                          const KeyPartition &part) {
  std::vector<std::vector<T>> value_buckets(part.num_buckets);
  uint64_t approximate_bucket_size = values.size() / part.num_buckets + 1;
  for (auto &vb : value_buckets) {
    vb.reserve(approximate_bucket_size);
  }
  for (size_t i = 0; i < values.size(); i++) {
    value_buckets[part.bucket_of_key[i]].push_back(values[i]);
  }
  return value_buckets;
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