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
  auto sparse_system = SparseSystem::make(num_variables);
  sparse_system->addEquation(0, {1, 2, 3}, 1);
  sparse_system->addEquation(1, {3, 4, 5}, 1);
  sparse_system->addEquation(2, {4, 5, 6}, 0);
  sparse_system->addEquation(3, {6, 7, 8}, 1);
  sparse_system->addEquation(4, {5, 8, 9}, 0);
  sparse_system->addEquation(5, {5, 8, 9}, 1); // Unsolvable duplicate equation.
  std::vector<uint32_t> equation_ids = {0, 1, 2, 3, 4, 5};
  ASSERT_THROW(lazyGaussianElimination(sparse_system, equation_ids),
               UnsolvableSystemException);
}

TEST(LazyGaussianEliminationTest, TestLazyFromDenseSolvableSystem) {
  uint32_t num_variables = 10;
  std::vector<std::pair<std::vector<uint32_t>, uint32_t>> equations = {
      {{1, 2, 3}, 1}, {{3, 4, 5}, 1}, {{4, 5, 6}, 0}, {{6, 7, 8}, 1},
      {{5, 8, 9}, 0}, {{0, 8, 9}, 1}, {{2, 8, 9}, 1}, {{0, 7, 9}, 1},
      {{1, 7, 9}, 0}, {{1, 2, 9}, 0}};

  std::vector<uint32_t> equation_ids = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  SparseSystemPtr sparse_system = SparseSystem::make(num_variables);

  for (uint32_t i = 0; i < equations.size(); i++) {
    sparse_system->addEquation(i, equations[i].first, equations[i].second);
  }

  auto [dense_ids, solved_ids, solved_vars, dense_system] =
      lazyGaussianElimination(sparse_system, equation_ids);
  BitArrayPtr dense_solution = gaussianElimination(dense_system, dense_ids);
  dense_solution =
      solveLazyFromDense(solved_ids, solved_vars, dense_system, dense_solution);

  verifySolution(sparse_system, dense_solution);
}

} // namespace caramel::tests