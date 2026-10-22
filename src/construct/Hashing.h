#pragma once

#include "SpookyHash.h"
#include <cstdint>
#include <string>

namespace caramel {

// One 128-bit signature per key drives everything: the bucket id (low half),
// the equation positions (rehash of the whole signature) and the prefilter
// probe (high half). The seed is fixed so every group, lane and worker agrees
// on a key's signature without coordination.
inline __uint128_t hashKey(const char *data, size_t length) {
  uint64_t hash1 = 0;
  uint64_t hash2 = 0;
  SpookyHash::Hash128(static_cast<const void *>(data), length, &hash1, &hash2);
  return (static_cast<__uint128_t>(hash1) << 64) | hash2;
}

inline __uint128_t hashKey(const std::string &key) {
  return hashKey(key.data(), key.size());
}

// The prefilter probe hash. Equal to SpookyHash::Hash64(key, 0), so it comes
// for free from the signature.
inline uint64_t filterHash(const __uint128_t &signature) {
  return static_cast<uint64_t>(signature >> 64);
}

inline uint64_t filterHash(const char *data, size_t length) {
  return SpookyHash::Hash64(static_cast<const void *>(data), length, 0);
}

inline uint32_t getBucketID(const __uint128_t &signature,
                            uint32_t num_buckets) {
  uint64_t bucket_hash = static_cast<uint64_t>(signature);
  return ((__uint128_t)(bucket_hash >> 1) * (__uint128_t)(num_buckets << 1)) >>
         64;
}

} // namespace caramel
