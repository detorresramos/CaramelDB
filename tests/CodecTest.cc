#include <gtest/gtest.h>
#include <random>
#include <src/Codec.h>

namespace caramel::tests {

std::vector<uint32_t> genRandomVector(uint32_t size) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint32_t> dist;

  std::vector<uint32_t> vector(size);
  for (size_t i = 0; i < size; ++i) {
    vector[i] = dist(gen);
  }

  return vector;
}

TEST(CodecTest, TestCannonicalHuffman) {
  uint32_t num_iters = 10;
  for (uint32_t iter = 0; iter < num_iters; iter++) {
    std::vector<uint32_t> symbols = genRandomVector(/* size = */ 30);

    auto [codedict, code_length_counts, sorted_symbols] =
        cannonicalHuffman(symbols);

    for (auto [expected_key, code] : codedict) {
      uint32_t actual_key =
          cannonicalDecode(code, code_length_counts, sorted_symbols);
      ASSERT_EQ(actual_key, expected_key);
    }
  }
}

} // namespace caramel::tests