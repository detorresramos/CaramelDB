#pragma once

#include <src/BitArray.h>
#include <src/Modulo2System.h>

namespace caramel {

std::tuple<std::vector<uint64_t>, std::vector<uint64_t>, std::vector<uint64_t>,
           DenseSystemPtr>
lazyGaussianElimination(const SparseSystemPtr &sparse_system,
                        const std::vector<uint64_t> &equation_ids);

BitArrayPtr solveLazyFromDense(const std::vector<uint64_t> &solved_ids,
                               const std::vector<uint64_t> &solved_vars,
                               const DenseSystemPtr &dense_system,
                               const BitArrayPtr &dense_solution);

} // namespace caramel