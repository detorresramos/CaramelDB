#include "BucketedHashStore.h"
#include "SpookyHash.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace caramel {

__uint128_t hashKey(const std::string &key, uint64_t seed) {
  const void *msgPtr = static_cast<const void *>(key.data());
  size_t length = key.size();
  uint64_t hash1 = seed;
  uint64_t hash2 = seed;
  SpookyHash::Hash128(msgPtr, length, &hash1, &hash2);
  return (static_cast<__uint128_t>(hash1) << 64) | hash2;
}

} // namespace caramel