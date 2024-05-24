#include "Modulo2System.h"
#include <map>
#include <stdexcept>
#include <unordered_set>

namespace caramel {

void DenseSystem::addEquation(
    uint32_t equation_id, const std::vector<uint32_t> &participating_variables,
    uint32_t constant) {
#ifdef DEBUG
  for (auto var : participating_variables) {
    if (var > _solution_size) {
      throw std::invalid_argument("Adding equation with var " +
                                  std::to_string(var) +
                                  " greater than solution size of " +
                                  std::to_string(_solution_size) + ".");
    }
  }
#endif

  BitArrayPtr equation = BitArray::make(_solution_size);
  for (auto var : participating_variables) {
    equation->setBit(var);
    // Note: If it simplifies / speeds up the code, we can insert by XOR:
    // equation = 0, then equation[v] ^= True for v in participating_variables.
    // This removes the need to de-dupe the input variables since an even number
    // of XORs will yield 0 and an odd number yields 1.
  }

  _equations[equation_id] = std::make_pair(std::move(equation), constant);
}

void DenseSystem::addEquation(
    uint32_t equation_id, const std::unordered_set<uint32_t> &participating_variables,
    uint32_t constant) {
#ifdef DEBUG
  for (auto var : participating_variables) {
    if (var > _solution_size) {
      throw std::invalid_argument("Adding equation with var " +
                                  std::to_string(var) +
                                  " greater than solution size of " +
                                  std::to_string(_solution_size) + ".");
    }
  }
#endif

  BitArrayPtr equation = BitArray::make(_solution_size);
  for (auto var : participating_variables) {
    equation->setBit(var);
    // Note: If it simplifies / speeds up the code, we can insert by XOR:
    // equation = 0, then equation[v] ^= True for v in participating_variables.
    // This removes the need to de-dupe the input variables since an even number
    // of XORs will yield 0 and an odd number yields 1.
  }

  _equations[equation_id] = std::make_pair(std::move(equation), constant);
}

void DenseSystem::xorEquations(uint32_t equation_to_modify,
                               uint32_t equation_to_xor) {
  auto &[equation_modify, constant_modify] = _equations[equation_to_modify];
  const auto &[equation_xor, constant_xor] = _equations[equation_to_xor];
  *equation_modify ^= *equation_xor;
  constant_modify ^= constant_xor;
}

void DenseSystem::swapEquations(uint32_t equation_id_1,
                                uint32_t equation_id_2) {
  auto temp_equation = _equations.find(equation_id_1)->second;
  _equations[equation_id_1] = _equations.find(equation_id_2)->second;
  _equations[equation_id_2] = temp_equation;
}

uint32_t DenseSystem::getFirstVar(uint32_t equation_id) {
  auto &[equation, constant] = _equations.find(equation_id)->second;
  // returns the first non-zero bit index in equation_id's equation
  std::optional<uint32_t> first_var = equation->find();
  // the equation is all 0s
  if (!first_var.has_value()) {
    if (constant) {
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
  std::map<uint32_t, std::pair<BitArrayPtr, uint32_t>> sorted_map(
      _equations.begin(), _equations.end());
  for (auto [equation_id, equation_and_constant] : sorted_map) {
    auto &[equation, constant] = equation_and_constant;
    output += equation->str() + " | " + std::to_string(constant) +
              "(Equation [" + std::to_string(equation_id) + "])\n";
  }
  return output;
}

DenseSystemPtr sparseToDense(const SparseSystemPtr &sparse_system) {
  uint32_t num_variables = sparse_system->solutionSize();

  DenseSystemPtr dense_system = DenseSystem::make(num_variables);

  std::vector<uint64_t> equation_ids = sparse_system->equationIds();
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