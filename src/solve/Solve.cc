#include "Solve.h"
#include "GaussianElimination.h"
#include "LazyGaussianElimination.h"

namespace caramel {

BitArrayPtr solveModulo2System(SparseSystem &sparse_system) {
  LazyGaussianOutput lazy_output =
      lazyGaussianElimination(sparse_system, equation_ids);

  BitArrayPtr solution = gaussianElimination(lazy_output.dense_system,
                                             lazy_output.dense_equation_ids);
}

} // namespace caramel