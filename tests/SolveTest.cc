#include <gtest/gtest.h>
#include <src/Modulo2System.h>
#include <src/solve/Solve.h>
#include "TestUtils.h"


namespace caramel::tests {

TEST(HypergraphPeelerTest, RandomSystemSolutions) {

  size_t num_systems = 1000; // Number of random systems to solve.
  uint32_t num_variables = 11;
  uint32_t num_equations = 10;

  size_t num_unsolvable = 0;

  for (size_t i = 0; i < num_systems; i++) {
    SparseSystemPtr sparse_system =
        genRandomSparseSystem(num_equations, num_variables);
    
    DenseSystemPtr dense_system = sparseToDense(sparse_system);

    try {
      BitArrayPtr solution = solveModulo2System(sparse_system);
      verifySolution(sparse_system, solution);
    }
    catch (UnsolvableSystemException& e) {
      num_unsolvable++;
      continue;
    }
  }
}


} // namespace caramel::tests