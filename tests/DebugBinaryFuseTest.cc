#include <gtest/gtest.h>
#include <src/construct/filter/BitPackedBinaryFuseFilter.h>
#include <iostream>

namespace caramel::tests {

TEST(DebugBinaryFuseTest, SmallDataset) {
  std::cout << "\n=== Testing BitPackedBinaryFuseFilter with 50 keys, 4-bit fingerprints ===" << std::endl;

  caramel::BitPackedBinaryFuseFilter filter(50, 4);

  // Generate 50 test hashes
  std::vector<uint64_t> keys;
  for (size_t i = 0; i < 50; i++) {
    keys.push_back(i * 123456789ULL);  // Simple hash function
  }

  std::cout << "Calling AddAll with " << keys.size() << " keys..." << std::endl;
  auto status = filter.AddAll(keys.data(), 0, keys.size());

  if (status == caramel::BinaryFuseStatus::Ok) {
    std::cout << "✅ Construction SUCCEEDED!" << std::endl;

    // Test a few lookups
    std::cout << "Testing lookups:" << std::endl;
    for (size_t i = 0; i < 5; i++) {
      auto result = filter.Contain(keys[i]);
      std::cout << "  Key[" << i << "]: " << (result == caramel::BinaryFuseStatus::Ok ? "FOUND" : "NOT FOUND") << std::endl;
    }

  } else if (status == caramel::BinaryFuseStatus::NotEnoughSpace) {
    std::cout << "❌ Construction FAILED: NotEnoughSpace" << std::endl;
    FAIL() << "Construction failed after all retry attempts";
  } else {
    std::cout << "❌ Construction FAILED: Unknown status" << std::endl;
    FAIL() << "Construction failed with unknown status";
  }
}

} // namespace caramel::tests
