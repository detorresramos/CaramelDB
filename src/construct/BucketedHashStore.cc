#include "BucketedHashStore.h"
#include "SpookyHash.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace caramel {

Uint128Signature hashKey(const std::string &key, uint64_t seed) {
  const void *msgPtr = static_cast<const void *>(key.data());
  size_t length = key.size();
  uint64_t hash1 = seed;
  uint64_t hash2 = seed;
  SpookyHash::Hash128(msgPtr, length, &hash1, &hash2);
  return {hash1, hash2};
}

uint32_t getBucketID(const Uint128Signature &signature, uint32_t num_buckets) {
  // Use first 64 bits of the signature to identify the segment
  uint64_t bucket_hash = signature.first;
  // This outputs a uniform integer from [0, self._num_buckets]
  uint32_t bucket_id =
      ((__uint128_t)(bucket_hash >> 1) * (__uint128_t)(num_buckets << 1)) >> 64;
  // TODO(any) should we rewrite this function to not use __uint128_t due to
  // platform incompatabilities?
  return bucket_id;
}

} // namespace caramel