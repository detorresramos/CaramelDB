#include "Modulo2System.h"
#include <map>
#include <unordered_set>

namespace caramel {

void DenseSystem::addEquation(
    uint32_t equation_id, const std::vector<uint32_t> &participating_variables,
    uint32_t constant) {
  for (auto var : participating_variables) {
    if (var > _solution_size) {
      throw std::invalid_argument("Adding equation with var " +
                                  std::to_string(var) +
                                  " greater than solution size of " +
                                  std::to_string(_solution_size) + ".");
    }
  }

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

std::string DenseSystem::str() const {
  std::string output;
  std::map<uint32_t, BitArrayPtr> sorted_map(_equations.begin(),
                                             _equations.end());
  for (auto [equation_id, equation] : sorted_map) {
    output += equation->str() + " | " +
              std::to_string(_constants.at(equation_id)) + "(Equation [" +
              std::to_string(equation_id) + "])\n";
  }
  return output;
}

DenseSystemPtr sparseToDense(const SparseSystemPtr &sparse_system) {
  uint32_t num_variables = sparse_system->solutionSize();

  DenseSystemPtr dense_system = DenseSystem::make(num_variables);

  std::vector<uint32_t> equation_ids = sparse_system->equationIds();
  for (auto equation_id : equation_ids) {
    auto [participating_vars, constant] =
        sparse_system->getEquation(equation_id);
    std::unordered_set<uint32_t> vars_to_add;
    for (auto variable_id : participating_vars) {
      if (!vars_to_add.count(variable_id)) {
        vars_to_add.insert(variable_id);
      } else {
        vars_to_add.erase(variable_id);
      }
    }
    // Update weight and priority for de-duped variables.
    dense_system->addEquation(
        equation_id,
        std::vector<uint32_t>(vars_to_add.begin(), vars_to_add.end()),
        constant);
  }

  return dense_system;
}

} // namespace caramel