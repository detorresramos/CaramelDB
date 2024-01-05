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

} // namespace caramel::tests