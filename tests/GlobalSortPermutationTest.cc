#include "TestUtils.h"
#include <gtest/gtest.h>
#include <random>
#include <src/construct/multiset/permute/GlobalSortPermutation.h>

namespace caramel::tests {

TEST(GlobalSortPermutationTest, CheckValidPermutation) {
  int num_cols = 10;
  int num_rows = 1000;
  int num_trials = 10;
  for (int i = 0; i < num_trials; i++) {
    std::vector<uint32_t> values = genRandomMatrix(num_rows, num_cols);
    std::vector<uint32_t> original(values);
    globalSortPermutation(values.data(), num_rows, num_cols);
    verifyValidPermutation(values, original, num_rows, num_cols);
  }
}

TEST(GlobalSortPermutationTest, PermutationDoesNotIncreaseEntropy) {
  int num_cols = 10;
  int num_rows = 1000;
  int num_trials = 10;
  for (int i = 0; i < num_trials; i++) {
    std::vector<uint32_t> values = genRandomMatrix(num_rows, num_cols);
    float original_entropy = computeColumnEntropy(values, num_rows, num_cols);
    globalSortPermutation(values.data(), num_rows, num_cols);
    float final_entropy = computeColumnEntropy(values, num_rows, num_cols);
    ASSERT_LE(final_entropy, original_entropy);
  }
}

TEST(GlobalSortPermutationTest, PermutationWithDuplicates) {
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
  globalSortPermutation(values.data(), num_rows, num_cols);
  float final_entropy = computeColumnEntropy(values, num_rows, num_cols);
  ASSERT_LE(final_entropy, original_entropy);
}

TEST(GlobalSortPermutationTest, PermutationDoesntRuinOptimum) {
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
  globalSortPermutation(values.data(), num_rows, num_cols);
  float final_entropy = computeColumnEntropy(values, num_rows, num_cols);
  ASSERT_NEAR(final_entropy, original_entropy, 0.001);
}

TEST(GlobalSortPermutationTest, RefinementImprovesOverSortAlone) {
  int num_cols = 10;
  int num_rows = 500;
  std::vector<uint32_t> values = genRandomMatrix(num_rows, num_cols);
  std::vector<uint32_t> sort_only(values);
  std::vector<uint32_t> sort_and_refine(values);

  globalSortPermutation(sort_only.data(), num_rows, num_cols, 0);
  globalSortPermutation(sort_and_refine.data(), num_rows, num_cols, 5);

  float sort_entropy = computeColumnEntropy(sort_only, num_rows, num_cols);
  float refined_entropy =
      computeColumnEntropy(sort_and_refine, num_rows, num_cols);
  ASSERT_LE(refined_entropy, sort_entropy);
}

} // namespace caramel::tests
