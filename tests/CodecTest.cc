#include "TestUtils.h"
#include <gtest/gtest.h>
#include <random>
#include <src/construct/CsfCodebook.h>

namespace caramel::tests {

TEST(CodecTest, TestSingleCanonicalHuffman) {
  std::vector<uint32_t> symbols = {2, 3, 4, 5, 6, 7, 3, 4, 5, 6};
  auto huffman_output = canonicalHuffman<uint32_t>(symbols);

  for (auto [expected_key, code] : huffman_output.codedict) {
    uint32_t actual_key =
        canonicalDecode<uint32_t>(code, huffman_output.code_length_counts,
                                   huffman_output.ordered_symbols);
    ASSERT_EQ(actual_key, expected_key);
  }
}

TEST(CodecTest, TestRandomCanonicalHuffman) {
  uint32_t num_iters = 10;
  for (uint32_t iter = 0; iter < num_iters; iter++) {
    std::vector<uint32_t> symbols = genRandomVector(/* size = */ 30);

    auto huffman_output = canonicalHuffman<uint32_t>(symbols);

    for (auto [expected_key, code] : huffman_output.codedict) {
      uint32_t actual_key =
          canonicalDecode<uint32_t>(code, huffman_output.code_length_counts,
                                     huffman_output.ordered_symbols);
      ASSERT_EQ(actual_key, expected_key);
    }
  }
}

TEST(CodecTest, TestRepeatItemCanonicalHuffman) {
  std::vector<uint32_t> symbols = {2, 2, 2, 2};
  auto huffman_output =
      canonicalHuffman<uint32_t>(symbols);

  for (auto [expected_key, code] : huffman_output.codedict) {
    uint32_t actual_key =
        canonicalDecode<uint32_t>(code, huffman_output.code_length_counts,
                                   huffman_output.ordered_symbols);
    ASSERT_EQ(actual_key, expected_key);
  }
}


// canonicalDecodeBranchless() is the query-time decoder; it must agree with the
// bit-at-a-time reference on every codeword, including when the low bits of the
// max_codelength-wide window are the arbitrary bits that follow the codeword in
// the solution array.
TEST(CodecTest, BranchlessDecodeMatchesReference) {
  std::mt19937_64 rng(7);
  for (uint32_t iter = 0; iter < 20; iter++) {
    std::vector<uint32_t> symbols = genRandomVector(/* size = */ 200);
    auto codebook = canonicalHuffman<uint32_t>(symbols);
    CanonicalDecodeTables tables;
    tables.build(codebook.code_length_counts, codebook.ordered_symbols.size(),
                 codebook.max_codelength);

    for (const auto &[symbol, code] : codebook.codedict) {
      for (uint32_t trial = 0; trial < 8; trial++) {
        uint32_t pad = codebook.max_codelength - code.numBits();
        uint64_t value = 0;
        for (uint32_t b = 0; b < code.numBits(); b++) {
          value = (value << 1) | code[b];
        }
        value <<= pad;
        value |= pad ? (rng() & ((UINT64_C(1) << pad) - 1)) : 0;

        uint32_t reference = canonicalDecodeFromNumber<uint32_t>(
            value, codebook.code_length_counts, codebook.ordered_symbols,
            codebook.max_codelength);
        uint32_t branchless = canonicalDecodeBranchless<uint32_t>(
            value, tables, codebook.ordered_symbols);
        ASSERT_EQ(branchless, reference);
        ASSERT_EQ(branchless, symbol);
      }
    }
  }
}

} // namespace caramel::tests