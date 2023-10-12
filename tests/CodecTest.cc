#include "TestUtils.h"
#include <gtest/gtest.h>
#include <random>
#include <src/construct/Codec.h>

namespace caramel::tests {

TEST(CodecTest, TestSingleCannonicalHuffman) {
  std::vector<uint32_t> symbols = {2, 3, 4, 5, 6, 7, 3, 4, 5, 6};
  auto [codedict, code_length_counts, sorted_symbols] =
      cannonicalHuffman<uint32_t>(symbols);

  for (auto [expected_key, code] : codedict) {
    uint32_t actual_key =
        cannonicalDecode<uint32_t>(code, code_length_counts, sorted_symbols);
    ASSERT_EQ(actual_key, expected_key);
  }
}

TEST(CodecTest, TestRandomCannonicalHuffman) {
  uint32_t num_iters = 10;
  for (uint32_t iter = 0; iter < num_iters; iter++) {
    std::vector<uint32_t> symbols = genRandomVector(/* size = */ 30);

    auto [codedict, code_length_counts, sorted_symbols] =
        cannonicalHuffman<uint32_t>(symbols);

    for (auto [expected_key, code] : codedict) {
      uint32_t actual_key =
          cannonicalDecode<uint32_t>(code, code_length_counts, sorted_symbols);
      ASSERT_EQ(actual_key, expected_key);
    }
  }
}

} // namespace caramel::tests