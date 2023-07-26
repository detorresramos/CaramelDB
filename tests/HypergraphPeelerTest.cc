#include <gtest/gtest.h>
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

  verify_peeling_order(unpeeled, peeled, order, sparse_system_output, equation_ids);
 
}

} // namespace caramel::tests