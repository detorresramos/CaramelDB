#include "Modulo2System.h"

namespace caramel {

void DenseSystem::addEquation(
    uint32_t equation_id, const std::vector<uint32_t> &participating_variables,
    uint32_t constant) {
  BitArrayPtr equation = BitArray::make(_solution_size);
  for (auto var : participating_variables) {
    equation->setBit(var);
  }

  _equations[equation_id] = equation;
  _constants[equation_id] = constant;
}

void DenseSystem::xorEquations(uint32_t equation_to_modify,
                               uint32_t equation_to_xor) {
  *_equations[equation_to_modify] ^= *_equations[equation_to_xor];
  _constants[equation_to_modify] ^= _constants[equation_to_xor];
}

void DenseSystem::swapEquations(uint32_t equation_id_1,
                                uint32_t equation_id_2) {
  BitArrayPtr temp_equation = _equations[equation_id_1];
  _equations[equation_id_1] = _equations[equation_id_2];
  _equations[equation_id_2] = temp_equation;

  uint32_t temp_constant = _constants[equation_id_1];
  _constants[equation_id_1] = _constants[equation_id_2];
  _constants[equation_id_2] = temp_constant;
}

uint32_t DenseSystem::getFirstVar(uint32_t equation_id) {
  // returns the first non-zero bit index in equation_id's equation
  std::optional<uint32_t> first_var = _equations[equation_id]->find();
  // the equation is all 0s
  if (!first_var.has_value()) {
    if (_constants[equation_id]) {
      // In this case, we have a linearly dependent row
      throw UnsolvableSystemException(
          "Can't find a 1 in the equation yet the constant is 1.");
    } else {
      // In this case, we have an identity row
      return _solution_size;
    }
  }

  return *first_var;
}

} // namespace caramel