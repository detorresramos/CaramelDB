#include "TestUtils.h"
#include <gtest/gtest.h>
#include <random>
#include <src/construct/EntropyPermutation.h>

namespace caramel::tests {


TEST(EntropyPermutationTest, CheckValidPermutation) {
  int num_cols = 10;
  int num_rows = 1000;
  int num_trials = 10;
  for (int i = 0; i < num_trials; i++) {
    std::vector<uint32_t> values = genRandomMatrix(num_rows, num_cols);
    // Manually deduplicate values.
    std::vector<uint32_t> permuted_values(values);
    entropyPermutation(values.data(), num_rows, num_cols);
    verifyValidPermutation(values, permuted_values, num_rows, num_cols);
  }
}

TEST(EntropyPermutationTest, PermutationDoesNotIncreaseEntropy) {
  int num_cols = 10;
  int num_rows = 1000;
  int num_trials = 10;
  for (int i = 0; i < num_trials; i++) {
    std::vector<uint32_t> values = genRandomMatrix(num_rows, num_cols);
    float original_entropy = computeColumnEntropy(values, num_rows, num_cols);
    entropyPermutation(values.data(), num_rows, num_cols);
    float final_entropy = computeColumnEntropy(values, num_rows, num_cols);
    ASSERT_LE(final_entropy, original_entropy);
  }
}

TEST(EntropyPermutationTest, PermutationWithDuplicates) {
  // Greedy should put together the duplicates and reach the optimum.
  std::vector<uint32_t> values = {
    0, 1, 0, 0,
    0, 0, 2, 0,
    4, 3, 0, 0,
    5, 0, 6, 0,
    7, 0, 8, 9,
    11, 10, 0, 12,
    13, 0, 14, 15,
  };
  int num_cols = 4;
  int num_rows = 7;
  float original_entropy = computeColumnEntropy(values, num_rows, num_cols);
  entropyPermutation(values.data(), num_rows, num_cols);
  float final_entropy = computeColumnEntropy(values, num_rows, num_cols);
  ASSERT_LE(final_entropy, original_entropy);
  ASSERT_NEAR(final_entropy, 6.993493337601384, 0.01);
}

TEST(EntropyPermutationTest, PermutationTopKEntropy) {
  // Greedy should hit the optimum entropy for the top num_columns values.
  // This matrix has maximum entropy (no permutation possible) for the values
  // outside the top-4.
  std::vector<uint32_t> values = {
    1, 0, 2, 3,
    0, 1, 3, 2,
    1, 0, 4, 2,
    0, 1, 5, 6,
    7, 0, 0, 8,
    9, 10, 11, 12,
  };
  int num_cols = 4;
  int num_rows = 6;
  float original_entropy = computeColumnEntropy(values, num_rows, num_cols);
  entropyPermutation(values.data(), num_rows, num_cols);
  float final_entropy = computeColumnEntropy(values, num_rows, num_cols);
  ASSERT_LE(final_entropy, original_entropy);
  ASSERT_NEAR(final_entropy, 5.945762006784577, 0.01);
}

TEST(EntropyPermutationTest, PermutationDoesntRuinOptimum) {
  // Greedy should not make changes (except permutation-invariant ones)
  // if we're at the optimum.
  std::vector<uint32_t> values = {
    1, 2, 4, 8,
    1, 2, 4, 8,
    1, 2, 5, 8,
    1, 2, 5, 8,
    1, 3, 6, 8,
    1, 3, 6, 7,
    1, 3, 7, 7,
    1, 3, 7, 7,
  };
  int num_cols = 4;
  int num_rows = 8;
  float original_entropy = computeColumnEntropy(values, num_rows, num_cols);
  entropyPermutation(values.data(), num_rows, num_cols);
  float final_entropy = computeColumnEntropy(values, num_rows, num_cols);
  ASSERT_NEAR(final_entropy, original_entropy, 0.001);
}

} // namespace caramel::tests