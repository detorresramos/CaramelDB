#include "Solve.h"
#include "GaussianElimination.h"
#include "HypergraphPeeler.h"
#include "LazyGaussianElimination.h"

namespace caramel {

BitArrayPtr solveModulo2System(const SparseSystemPtr &sparse_system) {

  auto [unpeeled_ids, peeled_ids, order, sparse_system_hg] =
      peelHypergraph(sparse_system, sparse_system->equationIds());

  auto [dense_ids, solved_ids, solved_vars, dense_system] =
      lazyGaussianElimination(sparse_system_hg, unpeeled_ids);

  BitArrayPtr dense_solution = gaussianElimination(dense_system, dense_ids);

  dense_solution =
      solveLazyFromDense(solved_ids, solved_vars, dense_system, dense_solution);

  dense_solution = 
    solvePeeledFromDense(peeled_ids, order, sparse_system_hg, dense_solution);

  return dense_solution;
}

} // namespace caramel