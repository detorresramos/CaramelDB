#include <gtest/gtest.h>
#include <src/construct/Construct.h>
#include <src/construct/filter/AutoFilterSelection.h>

namespace caramel::tests {

TEST(AutoFilterSelectionTest, BinaryFuseC_SmallN) {
  // For small n, C should be larger than asymptotic
  double c = binaryFuseC(100);
  EXPECT_GT(c, BINARY_FUSE_C_ASYMPTOTIC);
}

TEST(AutoFilterSelectionTest, BinaryFuseC_LargeN) {
  // For large n, should approach asymptotic value
  double c = binaryFuseC(10000000);
  EXPECT_NEAR(c, BINARY_FUSE_C_ASYMPTOTIC, 0.01);
}

TEST(AutoFilterSelectionTest, BinaryFuseC_EdgeCase) {
  EXPECT_DOUBLE_EQ(binaryFuseC(0), BINARY_FUSE_C_ASYMPTOTIC);
  EXPECT_DOUBLE_EQ(binaryFuseC(1), BINARY_FUSE_C_ASYMPTOTIC);
}

TEST(AutoFilterSelectionTest, LowerBound_HighAlpha) {
  // alpha=0.9, 1-bit xor: eps=0.5, b_eps=1.23, n_over_N=0.1
  double lb = autoLowerBound(0.9, 0.5, 1.23, 0.1);
  // = 1.089 * 0.9 * 0.5 - 0.1 - 1.23 * 0.1 = 0.49005 - 0.1 - 0.123 = 0.26705
  EXPECT_NEAR(lb, 0.26705, 0.001);
}

TEST(AutoFilterSelectionTest, LowerBound_LowAlpha) {
  // alpha=0.1, should be negative (filter not beneficial)
  double lb = autoLowerBound(0.1, 0.5, 1.23, 0.5);
  EXPECT_LT(lb, 0.0);
}

TEST(AutoFilterSelectionTest, BestDiscreteXor_HighAlpha) {
  auto result = bestDiscreteXor(0.9, 0.1);
  EXPECT_GE(result.param, 1);
  EXPECT_LE(result.param, 8);
  EXPECT_GT(result.lb, 0.0);
}

TEST(AutoFilterSelectionTest, BestDiscreteXor_LowAlpha) {
  auto result = bestDiscreteXor(0.1, 0.5);
  EXPECT_LT(result.lb, 0.0);
}

TEST(AutoFilterSelectionTest, BestDiscreteBinaryFuse_HighAlpha) {
  auto result = bestDiscreteBinaryFuse(0.9, 0.1, 1000);
  EXPECT_GE(result.param, 1);
  EXPECT_LE(result.param, 8);
  EXPECT_GT(result.lb, 0.0);
}

TEST(AutoFilterSelectionTest, BestDiscreteBloomAllK_HighAlpha) {
  auto result = bestDiscreteBloomAllK(0.9, 0.1);
  EXPECT_GE(result.bpe, 1);
  EXPECT_LE(result.bpe, 8);
  EXPECT_GE(result.k, 1);
  EXPECT_LE(result.k, 8);
  EXPECT_GT(result.lb, 0.0);
}

TEST(AutoFilterSelectionTest, SelectBestFilter_HighAlpha) {
  std::vector<uint32_t> values;
  for (size_t i = 0; i < 9000; i++) values.push_back(42);
  for (size_t i = 0; i < 1000; i++) values.push_back(static_cast<uint32_t>(i));

  auto config = selectBestFilter<uint32_t>(values, true);
  ASSERT_NE(config, nullptr);
}

TEST(AutoFilterSelectionTest, SelectBestFilter_Uniform) {
  std::vector<uint32_t> values;
  for (size_t i = 0; i < 1000; i++) values.push_back(static_cast<uint32_t>(i));

  auto config = selectBestFilter<uint32_t>(values);
  ASSERT_EQ(config, nullptr);
}

TEST(AutoFilterSelectionTest, SelectBestFilter_AllIdentical) {
  std::vector<uint32_t> values(1000, 7);
  auto config = selectBestFilter<uint32_t>(values);
  ASSERT_EQ(config, nullptr);
}

TEST(AutoFilterSelectionTest, EndToEnd_SkewedUsesFilter) {
  size_t n = 5000;
  std::vector<std::string> keys;
  std::vector<uint32_t> values;
  keys.reserve(n);
  values.reserve(n);

  // 90% one value, 10% unique values
  for (size_t i = 0; i < n * 9 / 10; i++) {
    keys.push_back("key_" + std::to_string(i));
    values.push_back(42);
  }
  for (size_t i = n * 9 / 10; i < n; i++) {
    keys.push_back("key_" + std::to_string(i));
    values.push_back(static_cast<uint32_t>(i));
  }

  auto auto_config = std::make_shared<AutoPreFilterConfig>();
  CsfPtr<uint32_t> csf =
      constructCsf<uint32_t>(keys, values, auto_config, false);

  for (size_t i = 0; i < n; i++) {
    ASSERT_EQ(csf->query(keys[i]), values[i]);
  }
  ASSERT_NE(csf->getFilter(), nullptr);
}

TEST(AutoFilterSelectionTest, EndToEnd_UniformNoFilter) {
  size_t n = 1000;
  std::vector<std::string> keys;
  std::vector<uint32_t> values;
  keys.reserve(n);
  values.reserve(n);

  for (size_t i = 0; i < n; i++) {
    keys.push_back("key_" + std::to_string(i));
    values.push_back(static_cast<uint32_t>(i));
  }

  auto auto_config = std::make_shared<AutoPreFilterConfig>();
  CsfPtr<uint32_t> csf =
      constructCsf<uint32_t>(keys, values, auto_config, false);

  for (size_t i = 0; i < n; i++) {
    ASSERT_EQ(csf->query(keys[i]), values[i]);
  }
  ASSERT_EQ(csf->getFilter(), nullptr);
}

TEST(AutoFilterSelectionTest, EndToEnd_AllIdenticalNoFilter) {
  size_t n = 500;
  std::vector<std::string> keys;
  std::vector<uint32_t> values;

  for (size_t i = 0; i < n; i++) {
    keys.push_back("key_" + std::to_string(i));
    values.push_back(7);
  }

  auto auto_config = std::make_shared<AutoPreFilterConfig>();
  CsfPtr<uint32_t> csf =
      constructCsf<uint32_t>(keys, values, auto_config, false);

  for (size_t i = 0; i < n; i++) {
    ASSERT_EQ(csf->query(keys[i]), 7);
  }
  ASSERT_EQ(csf->getFilter(), nullptr);
}

} // namespace caramel::tests
