#pragma once

#include <src/BitArray.h>
#include <src/Modulo2System.h>

namespace caramel {

BitArrayPtr
gaussianElimination(DenseSystem &dense_system,
                    const std::vector<uint32_t> &relevant_equation_ids);
}