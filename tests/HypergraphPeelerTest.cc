#include "TestUtils.h"
#include <gtest/gtest.h>
#include <src/Modulo2System.h>
#include <src/solve/GaussianElimination.h>
#include <src/solve/HypergraphPeeler.h>

namespace caramel::tests {

TEST(HypergraphPeelerTest, PeelSimpleHypergraph) {
  uint32_t num_variables = 10;
  std::vector<std::vector<uint64_t>> equations = {
      {1, 2, 3}, {3, 4, 5}, {4, 5, 6}, {6, 7, 8}, {5, 8, 9}, {0, 8, 9}};
  std::vector<uint32_t> constants = {1, 1, 0, 1, 0, 0};

  auto sparse_system = SparseSystem::make(equations.size(), num_variables);
  for (uint32_t i = 0; i < equations.size(); i++) {
    sparse_system->addTestEquation(equations[i], constants[i]);
  }

  std::vector<uint64_t> equation_ids = {0, 1, 2, 3, 4, 5};

  auto [unpeeled, peeled, order, sparse_system_output] =
      peelHypergraph(sparse_system, equation_ids);

  verify_peeling_order(unpeeled, peeled, order, sparse_system_output,
                       equation_ids);
}

TEST(HypergraphPeelerTest, PeelDoubleEdgedHypergraph) {
  // Test the situation where the graph edges have duplicates.
  uint32_t num_variables = 11;

  std::vector<std::vector<uint64_t>> equations = {
      {2, 5, 10}, {3, 3, 3}, {6, 6, 6}, {8, 8, 8},  {0, 0, 0},
      {3, 5, 6},  {4, 4, 4}, {0, 1, 3}, {6, 9, 10}, {3, 6, 10}};
  std::vector<uint32_t> constants = {1, 1, 0, 1, 0, 1, 0, 1, 0, 1};

  auto sparse_system = SparseSystem::make(equations.size(), num_variables);
  for (uint32_t i = 0; i < equations.size(); i++) {
    sparse_system->addTestEquation(equations[i], constants[i]);
  }

  std::vector<uint64_t> equation_ids = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

  auto [unpeeled, peeled, order, sparse_system_output] =
      peelHypergraph(sparse_system, equation_ids);

  verify_peeling_order(unpeeled, peeled, order, sparse_system_output,
                       equation_ids);
}

TEST(HypergraphPeelerTest, TestPeeledFromDenseSolvableSystemDoubleEdged) {
  uint32_t num_variables = 11;
  std::vector<std::vector<uint64_t>> equations = {
      {2, 5, 10}, {3, 3, 3}, {6, 6, 6}, {8, 8, 8},  {0, 0, 0},
      {3, 5, 6},  {4, 4, 4}, {0, 1, 3}, {6, 9, 10}, {3, 6, 10}};
  std::vector<uint32_t> constants = {1, 1, 0, 1, 0, 1, 0, 1, 0, 1};

  auto sparse_system = SparseSystem::make(equations.size(), num_variables);
  for (uint32_t i = 0; i < equations.size(); i++) {
    sparse_system->addTestEquation(equations[i], constants[i]);
  }

  std::vector<uint64_t> equation_ids = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

  auto [unpeeled, peeled, order, sparse_system_output] =
      peelHypergraph(sparse_system, equation_ids);

  verify_peeling_order(unpeeled, peeled, order, sparse_system_output,
                       equation_ids);

  DenseSystemPtr dense_system = sparseToDense(sparse_system);

  BitArrayPtr solution = gaussianElimination(dense_system, unpeeled);

  solution = solvePeeledFromDense(peeled, order, sparse_system, solution);

  verifySolution(sparse_system, solution);
}

TEST(HypergraphPeelerTest, TestPeeledFromDenseSolvableSystem) {
  uint32_t num_variables = 10;
  std::vector<std::vector<uint64_t>> equations = {
      {1, 2, 3}, {3, 4, 5}, {4, 5, 6}, {6, 7, 8}, {5, 8, 9},
      {0, 8, 9}, {2, 8, 9}, {0, 7, 9}, {1, 7, 9}, {1, 2, 9}};
  std::vector<uint32_t> constants = {1, 1, 0, 1, 0, 1, 1, 1, 0, 0};

  auto sparse_system = SparseSystem::make(equations.size(), num_variables);
  for (uint32_t i = 0; i < equations.size(); i++) {
    sparse_system->addTestEquation(equations[i], constants[i]);
  }

  std::vector<uint64_t> equation_ids = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

  auto [unpeeled, peeled, order, sparse_system_output] =
      peelHypergraph(sparse_system, equation_ids);

  verify_peeling_order(unpeeled, peeled, order, sparse_system_output,
                       equation_ids);

  DenseSystemPtr dense_system = sparseToDense(sparse_system);

  BitArrayPtr solution = gaussianElimination(dense_system, unpeeled);

  solution = solvePeeledFromDense(peeled, order, sparse_system, solution);

  verifySolution(sparse_system, solution);
}

} // namespace caramel::tests