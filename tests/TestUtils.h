#pragma once
#include <gtest/gtest.h>
#include <random>
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

inline std::vector<uint32_t> genRandomVector(uint32_t size) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint32_t> dist(0, 100);

  std::vector<uint32_t> vector(size);
  for (size_t i = 0; i < size; ++i) {
    vector[i] = dist(gen);
  }

  return vector;
}

} // namespace caramel::tests