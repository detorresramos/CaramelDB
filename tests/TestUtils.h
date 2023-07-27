#pragma once
#include <gtest/gtest.h>
#include <src/Modulo2System.h>

namespace caramel::tests {

inline void verifySolution(const SparseSystemPtr &original_sparse_system,
                           const BitArrayPtr &solution) {
  DenseSystemPtr original_system = sparseToDense(original_sparse_system);
  for (auto equation_id : original_sparse_system->equationIds()) {
    auto [equation, constant] = original_system->getEquation(equation_id);
    ASSERT_EQ(BitArray::scalarProduct(equation, solution), constant);
  }
}

} // namespace caramel::tests