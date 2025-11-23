#include <gtest/gtest.h>
#include <deps/fastfilter_cpp/src/xorfilter/4wise_xor_binary_fuse_filter_lowmem.h>
#include <iostream>
#include <vector>

namespace caramel::tests {

// Test using the actual library implementation
TEST(LibraryBinaryFuseTest, SmallDataset) {
  std::cout << "\n=== Testing Library Binary Fuse Filter with 50 keys ===\n";

  // Create filter using library implementation
  typedef xorbinaryfusefilter_lowmem4wise::XorBinaryFuseFilter<uint64_t, uint8_t> BinaryFuse8;
  BinaryFuse8 filter(50);

  std::cout << "[LIBRARY] arrayLength=" << filter.arrayLength
            << ", segmentLength=" << filter.segmentLength
            << ", segmentCount=" << filter.segmentCount
            << ", segmentCountLength=" << filter.segmentCountLength << std::endl;

  // Generate 50 test hashes
  std::vector<uint64_t> keys;
  for (size_t i = 0; i < 50; i++) {
    keys.push_back(i * 123456789ULL);
  }

  std::cout << "Calling AddAll with " << keys.size() << " keys...\n";
  auto status = filter.AddAll(keys.data(), 0, keys.size());

  if (status == xorbinaryfusefilter_lowmem4wise::Ok) {
    std::cout << "✅ Library construction SUCCEEDED!\n";

    // Test lookups
    for (size_t i = 0; i < 5; i++) {
      auto result = filter.Contain(keys[i]);
      std::cout << "  Key[" << i << "]: " << (result == xorbinaryfusefilter_lowmem4wise::Ok ? "FOUND" : "NOT FOUND") << std::endl;
    }

    SUCCEED();
  } else {
    std::cout << "❌ Library construction FAILED\n";
    FAIL() << "Library construction failed";
  }
}

} // namespace caramel::tests
