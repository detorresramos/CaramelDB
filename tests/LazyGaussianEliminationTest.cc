#include "TestUtils.h"
#include <gtest/gtest.h>
#include <memory>
#include <src/solve/GaussianElimination.h>
#include <src/solve/LazyGaussianElimination.h>
#include <tuple>

namespace caramel::tests {

TEST(LazyGaussianEliminationTest, TestUnsolvablePair) {

  // Tests the case where two equations have incompatible constants.
  uint32_t num_variables = 10;

  std::vector<std::vector<uint64_t>> equations = {
      {1, 2, 3}, {3, 4, 5}, {4, 5, 6},
      {6, 7, 8}, {5, 8, 9}, {5, 8, 9} // Unsolvable duplicate equation.
  };

  std::vector<uint32_t> constants = {1, 1, 0, 1, 0, 1};

  auto sparse_system = SparseSystem::make(equations.size(), num_variables);
  for (uint32_t i = 0; i < equations.size(); i++) {
    sparse_system->addTestEquation(equations[i], constants[i]);
  }

  std::vector<uint64_t> equation_ids = {0, 1, 2, 3, 4, 5};
  ASSERT_THROW(lazyGaussianElimination(sparse_system, equation_ids),
               UnsolvableSystemException);
}

TEST(LazyGaussianEliminationTest, TestLazyFromDenseSolvableSystem) {
  uint32_t num_variables = 10;
  std::vector<std::vector<uint64_t>> equations = {
      {1, 2, 3}, {3, 4, 5}, {4, 5, 6}, {6, 7, 8}, {5, 8, 9},
      {0, 8, 9}, {2, 8, 9}, {0, 7, 9}, {1, 7, 9}, {1, 2, 9}};

  std::vector<uint32_t> constants = {1, 1, 0, 1, 0, 1, 1, 1, 0, 0};

  std::vector<uint64_t> equation_ids = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  auto sparse_system = SparseSystem::make(equations.size(), num_variables);
  for (uint32_t i = 0; i < equations.size(); i++) {
    sparse_system->addTestEquation(equations[i], constants[i]);
  }

  auto [dense_ids, solved_ids, solved_vars, dense_system] =
      lazyGaussianElimination(sparse_system, equation_ids);
  BitArrayPtr dense_solution = gaussianElimination(dense_system, dense_ids);
  dense_solution =
      solveLazyFromDense(solved_ids, solved_vars, dense_system, dense_solution);

  verifySolution(sparse_system, dense_solution);
}

} // namespace caramel::tests