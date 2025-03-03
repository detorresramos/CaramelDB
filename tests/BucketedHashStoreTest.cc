#include "TestUtils.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <random>
#include <set>
#include <src/construct/BucketedHashStore.h>
#include <stdexcept>

namespace caramel::tests {

std::string genRandomString(size_t length = 10) {
  auto randchar = []() -> char {
    const char charset[] = "0123456789"
                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                           "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<int> dist(0, max_index);
    return charset[dist(mt)];
  };
  std::string str(length, 0);
  std::generate_n(str.begin(), length, randchar);
  return str;
}

std::vector<std::string> manyRandomStrings(uint32_t num_strings = 10) {
  std::vector<std::string> strings;
  for (uint32_t i = 0; i < num_strings; i++) {
    strings.push_back(genRandomString());
  }
  return strings;
}

TEST(BucketedHashStoreTest, TestGetBucketID) {
  std::vector<__uint128_t> signatures;
  for (auto key : manyRandomStrings(100)) {
    signatures.push_back(hashKey(key, 341));
  }

  for (uint32_t num_buckets = 10; num_buckets < 20; num_buckets++) {
    std::set<uint32_t> unique_buckets;
    for (auto signature : signatures) {
      uint32_t bucket_id = getBucketID(signature, num_buckets);
      unique_buckets.insert(bucket_id);
      ASSERT_LE(bucket_id, num_buckets);
    }
    ASSERT_GE(unique_buckets.size(), num_buckets / 2);
  }
}

TEST(BucketedHashStoreTest, TestPartitioning) {
  uint32_t size = 100;
  auto keys = manyRandomStrings(size);
  auto values = genRandomVector(size);

  auto hash_store =
      partitionToBuckets<uint32_t>(keys, values, 20);

  for (auto key : keys) {
    __uint128_t signature = hashKey(key, 0);
    uint32_t bucket_id = getBucketID(signature, 6);
    if (std::find(hash_store.key_buckets[bucket_id].begin(), hash_store.key_buckets[bucket_id].end(),
                  signature) == hash_store.key_buckets[bucket_id].end()) {
      throw std::invalid_argument("Key not found in its bucket.");
    }
  }
}

TEST(BucketedHashStoreTest, TestDuplicateKey) {
  std::vector<std::string> keys = {"HAHA", "HAHA"};
  auto values = genRandomVector(2);

  ASSERT_THROW(partitionToBuckets<uint32_t>(keys, values, 1),
               std::runtime_error);
}

} // namespace caramel::tests