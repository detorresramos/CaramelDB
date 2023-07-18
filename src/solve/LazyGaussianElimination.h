#pragma once

#include <src/BitArray.h>
#include <src/Modulo2System.h>

namespace caramel {

std::tuple<std::vector<uint32_t>, std::vector<uint32_t>, std::vector<uint32_t>,
           DenseSystemPtr>
lazyGaussianElimination(SparseSystemPtr &sparse_system,
                        const std::vector<uint32_t> &equation_ids);

BitArrayPtr solveLazyFromDense(const std::vector<uint32_t> &solved_ids,
                               const std::vector<uint32_t> &solved_vars,
                               const DenseSystemPtr &dense_system,
                               const BitArrayPtr &dense_solution);

} // namespace caramel