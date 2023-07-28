#include <gtest/gtest.h>
#include <src/Modulo2System.h>
#include <src/solve/GaussianElimination.h>
#include <src/solve/HypergraphPeeler.h>
#include "TestUtils.h"

namespace caramel::tests {

TEST(HypergraphPeelerTest, PeelSimpleHypergraph) {
  uint32_t num_variables = 10;
  auto sparse_system = SparseSystem::make(num_variables);
  sparse_system->addEquation(0, {1, 2, 3}, 1);
  sparse_system->addEquation(1, {3, 4, 5}, 1);
  sparse_system->addEquation(2, {4, 5, 6}, 0);
  sparse_system->addEquation(3, {6, 7, 8}, 1);
  sparse_system->addEquation(4, {5, 8, 9}, 0);
  sparse_system->addEquation(5, {0, 8, 9}, 0);
  std::vector<uint32_t> equation_ids = {0, 1, 2, 3, 4, 5};

  auto [unpeeled, peeled, order, sparse_system_output] =
      peelHypergraph(sparse_system, equation_ids);

  verify_peeling_order(
    unpeeled, peeled, order, sparse_system_output, equation_ids);
}

TEST(HypergraphPeelerTest, PeelDoubleEdgedHypergraph) {
  // Test the situation where the graph edges have duplicates.
  uint32_t num_variables = 11;
  auto sparse_system = SparseSystem::make(num_variables);
  sparse_system->addEquation(0, {2, 5, 10}, 1);
  sparse_system->addEquation(1, {3, 3, 3}, 1);
  sparse_system->addEquation(2, {6, 6, 6}, 0);
  sparse_system->addEquation(3, {8, 8, 8}, 1);
  sparse_system->addEquation(4, {0, 0, 0}, 0);
  sparse_system->addEquation(5, {3, 5, 6}, 1);
  sparse_system->addEquation(6, {4, 4, 4}, 0);
  sparse_system->addEquation(7, {0, 1, 3}, 1);
  sparse_system->addEquation(8, {6, 9, 10}, 0);
  sparse_system->addEquation(9, {3, 6, 10}, 1);

  std::vector<uint32_t> equation_ids =
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

  auto [unpeeled, peeled, order, sparse_system_output] =
      peelHypergraph(sparse_system, equation_ids);

  verify_peeling_order(
    unpeeled, peeled, order, sparse_system_output, equation_ids);
}


TEST(HypergraphPeelerTest, TestPeeledFromDenseSolvableSystemDoubleEdged) {
  uint32_t num_variables = 11;
  auto sparse_system = SparseSystem::make(num_variables);
  sparse_system->addEquation(0, {2, 5, 10}, 1);
  sparse_system->addEquation(1, {3, 3, 3}, 1);
  sparse_system->addEquation(2, {6, 6, 6}, 0);
  sparse_system->addEquation(3, {8, 8, 8}, 1);
  sparse_system->addEquation(4, {0, 0, 0}, 0);
  sparse_system->addEquation(5, {3, 5, 6}, 1);
  sparse_system->addEquation(6, {4, 4, 4}, 0);
  sparse_system->addEquation(7, {0, 1, 3}, 1);
  sparse_system->addEquation(8, {6, 9, 10}, 0);
  sparse_system->addEquation(9, {3, 6, 10}, 1);

  std::vector<uint32_t> equation_ids =
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

  auto [unpeeled, peeled, order, sparse_system_output] =
      peelHypergraph(sparse_system, equation_ids);

  verify_peeling_order(
    unpeeled, peeled, order, sparse_system_output, equation_ids);

  DenseSystemPtr dense_system = sparseToDense(sparse_system);

  BitArrayPtr solution = gaussianElimination(dense_system, unpeeled);

  solution = solvePeeledFromDense(peeled, order, sparse_system, solution);

  verifySolution(sparse_system, solution);
}



TEST(HypergraphPeelerTest, TestPeeledFromDenseSolvableSystem) {
  uint32_t num_variables = 10;
  auto sparse_system = SparseSystem::make(num_variables);
  sparse_system->addEquation(0, {1, 2, 3}, 1);
  sparse_system->addEquation(1, {3, 4, 5}, 1);
  sparse_system->addEquation(2, {4, 5, 6}, 0);
  sparse_system->addEquation(3, {6, 7, 8}, 1);
  sparse_system->addEquation(4, {5, 8, 9}, 0);
  sparse_system->addEquation(5, {0, 8, 9}, 1);
  sparse_system->addEquation(6, {2, 8, 9}, 1);
  sparse_system->addEquation(7, {0, 7, 9}, 1);
  sparse_system->addEquation(8, {1, 7, 9}, 0);
  sparse_system->addEquation(9, {1, 2, 9}, 0);

  std::vector<uint32_t> equation_ids = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

  auto [unpeeled, peeled, order, sparse_system_output] =
      peelHypergraph(sparse_system, equation_ids);

  verify_peeling_order(
    unpeeled, peeled, order, sparse_system_output, equation_ids);

  DenseSystemPtr dense_system = sparseToDense(sparse_system);

  BitArrayPtr solution = gaussianElimination(dense_system, unpeeled);

  solution = solvePeeledFromDense(peeled, order, sparse_system, solution);

  verifySolution(sparse_system, solution);
}

} // namespace caramel::tests