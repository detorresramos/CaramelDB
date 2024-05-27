#pragma once

#include <src/BitArray.h>
#include <src/Modulo2System.h>

namespace caramel {

void lazyGaussianElimination(const SparseSystemPtr &sparse_system,
                             const std::vector<uint64_t> &equation_ids,
                             std::vector<uint64_t> dense_ids,
                             std::vector<uint64_t> solved_ids,
                             std::vector<uint64_t> solved_vars,
                             DenseSystemPtr dense_system);

BitArrayPtr solveLazyFromDense(const std::vector<uint64_t> &solved_ids,
                               const std::vector<uint64_t> &solved_vars,
                               const DenseSystemPtr &dense_system,
                               const BitArrayPtr &dense_solution);

} // namespace caramel