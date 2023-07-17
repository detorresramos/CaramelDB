#include <gtest/gtest.h>
#include <memory>
#include <src/solve/LazyGaussianElimination.h>
#include <tuple>

namespace caramel::tests {

TEST(LazyGaussianEliminationTest, TestUnsolvablePair) {

  // Tests the case where two equations have incompatible constants.
  uint32_t num_variables = 10;
  auto sparse_system = std::make_shared<SparseSystem>(num_variables);
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

} // namespace caramel::tests