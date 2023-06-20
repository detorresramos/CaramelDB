#pragma once

#include "BitArray.h"
#include "Modulo2System.h"

namespace caramel {

BitArray
gaussianElimination(DenseSystem &dense_system,
                    const std::vector<uint32_t> &relevant_equation_ids);
}