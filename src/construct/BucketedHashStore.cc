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

} // namespace caramel