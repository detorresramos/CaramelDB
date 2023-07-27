#include "Solve.h"
#include "GaussianElimination.h"
#include "LazyGaussianElimination.h"

namespace caramel {

BitArrayPtr solveModulo2System(const SparseSystemPtr &sparse_system) {
  // TODO hypergraph peeling
  std::vector<uint32_t> unpeeled_ids = sparse_system->equationIds();

  auto [dense_ids, solved_ids, solved_vars, dense_system] =
      lazyGaussianElimination(sparse_system, unpeeled_ids);

  BitArrayPtr dense_solution = gaussianElimination(dense_system, dense_ids);

  dense_solution =
      solveLazyFromDense(solved_ids, solved_vars, dense_system, dense_solution);

  return dense_solution;
}

} // namespace caramel