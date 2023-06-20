#include "Modulo2System.h"

namespace caramel {

void DenseSystem::addEquation(uint32_t equation_id,
                 std::vector<uint32_t> participating_variables,
                 uint32_t constant);

std::pair<BitArray, uint32_t> DenseSystem::getEquation(uint32_t equation_id);

void DenseSystem::xorEquations(uint32_t equation_to_modify, uint32_t equation_to_xor);

void DenseSystem::swapEquations(uint32_t equation_id_1, uint32_t equation_id_2);

uint32_t DenseSystem::getFirstVar(uint32_t equation_id);

} // namespace caramel