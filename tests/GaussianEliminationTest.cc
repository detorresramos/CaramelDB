#include <gtest/gtest.h>
#include <memory>
#include <src/solve/GaussianElimination.h>

namespace caramel::tests {

TEST(GaussianEliminationTest, TestSimple) {
  std::vector<std::vector<uint32_t>> matrix = {{0, 1, 2}, {1, 2}, {0, 2}};
  std::vector<uint32_t> constants = {1, 0, 1};
  std::string solution_str = "100";

  auto system = DenseSystem(solution_str.size());
  for (uint32_t i = 0; i < matrix.size(); i++) {
    system.addEquation(/* equation_id = */ i,
                       /* participating_variables = */ matrix[i],
                       /* constant = */ constants[i]);
  }

  ASSERT_EQ(gaussianElimination(system, {2, 0, 1})->str(), solution_str);
}

TEST(GaussianEliminationTest, TestWithSwaps) {
  std::vector<std::vector<uint32_t>> matrix = {{1, 34}, {0, 1, 34}, {0, 34}};
  std::vector<uint32_t> constants = {0, 1, 1};
  std::string solution_str = "1";
  for (uint32_t i = 0; i < 34; i++) {
    solution_str += "0";
  }

  auto system = DenseSystem(solution_str.size());
  for (uint32_t i = 0; i < matrix.size(); i++) {
    system.addEquation(/* equation_id = */ i,
                       /* participating_variables = */ matrix[i],
                       /* constant = */ constants[i]);
  }

  ASSERT_EQ(gaussianElimination(system, {0, 1, 2})->str(), solution_str);
}

TEST(GaussianEliminationTest, TestEmpty) {
  std::vector<std::vector<uint32_t>> matrix = {{1, 34}, {0, 1, 34}, {0, 34}};
  std::vector<uint32_t> constants = {0, 1, 1};
  std::string solution_str;
  for (uint32_t i = 0; i < 35; i++) {
    solution_str += "0";
  }

  auto system = DenseSystem(solution_str.size());
  for (uint32_t i = 0; i < matrix.size(); i++) {
    system.addEquation(/* equation_id = */ i,
                       /* participating_variables = */ matrix[i],
                       /* constant = */ constants[i]);
  }

  ASSERT_EQ(gaussianElimination(system, {})->str(), solution_str);
}

TEST(GaussianEliminationTest, TestUnsolvable) {
  std::vector<std::vector<uint32_t>> matrix = {{0, 1}, {0, 1}, {0, 2}};
  std::vector<uint32_t> constants = {0, 1, 1};

  auto system = DenseSystem(3);
  for (uint32_t i = 0; i < matrix.size(); i++) {
    system.addEquation(/* equation_id = */ i,
                       /* participating_variables = */ matrix[i],
                       /* constant = */ constants[i]);
  }

  ASSERT_THROW(gaussianElimination(system, {0, 1, 2}), UnsolvableSystemException);
}

TEST(GaussianEliminationTest, TestSettingMultipleSolutionBits) {
  std::vector<std::vector<uint32_t>> matrix = {{0, 1}, {1, 2}, {2, 3}, {3, 4}};
  std::vector<uint32_t> constants = {1, 1, 1, 1};
  std::string solution_str = "01010";

  auto system = DenseSystem(solution_str.size());
  for (uint32_t i = 0; i < matrix.size(); i++) {
    system.addEquation(/* equation_id = */ i,
                       /* participating_variables = */ matrix[i],
                       /* constant = */ constants[i]);
  }

  ASSERT_EQ(gaussianElimination(system, {0, 1, 2, 3})->str(), solution_str);
}

} // namespace caramel::tests