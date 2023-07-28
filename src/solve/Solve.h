#pragma once

#include <src/BitArray.h>
#include <src/Modulo2System.h>

namespace caramel {

BitArrayPtr solveModulo2System(const SparseSystemPtr &sparse_system);

} // namespace caramel