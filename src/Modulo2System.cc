#include "Modulo2System.h"
#include <map>
#include <stdexcept>
#include <unordered_set>

namespace caramel {

void DenseSystem::addEquation(
    uint64_t equation_id, const std::vector<uint64_t> &participating_variables,
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

  BitArray equation = getEquation(equation_id);
  uint64_t &eq_constant = getConstant(equation_id);
  uint64_t &first_var = getFirstVar(equation_id);

  for (auto var : participating_variables) {
    equation.setBit(var);
  }

  eq_constant = constant;
  first_var = 0;
}

void DenseSystem::addEquation(
    uint64_t equation_id,
    const std::unordered_set<uint64_t> &participating_variables,
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

  BitArray equation = getEquation(equation_id);
  uint64_t &eq_constant = getConstant(equation_id);
  uint64_t &first_var = getFirstVar(equation_id);

  for (auto var : participating_variables) {
    equation.setBit(var);
  }

  eq_constant = constant;
  first_var = 0;
}

void DenseSystem::addEquation(uint64_t equation_id,
                              const uint64_t *equation_ptr, uint32_t constant) {
#ifdef DEBUG
  for (const uint64_t *var_id = equation_ptr; var_id < equation_ptr + 3;
       ++var_id) {
    if (*var_id > _solution_size) {
      throw std::invalid_argument("Adding equation with var " +
                                  std::to_string(*var_id) +
                                  " greater than solution size of " +
                                  std::to_string(_solution_size) + ".");
    }
  }
#endif

  BitArray equation = getEquation(equation_id);
  uint64_t &eq_constant = getConstant(equation_id);
  uint64_t &first_var = getFirstVar(equation_id);

  for (const uint64_t *var_id = equation_ptr; var_id < equation_ptr + 3;
       ++var_id) {
    equation.setBit(*var_id);
  }

  eq_constant = constant;
  first_var = 0;
}

void DenseSystem::xorEquations(uint64_t equation_to_modify,
                               uint64_t equation_to_xor) {
  BitArray equation_modify = getEquation(equation_to_modify);
  uint64_t &constant_modify = getConstant(equation_to_modify);
  BitArray equation_xor = getEquation(equation_to_xor);
  uint64_t &constant_xor = getConstant(equation_to_xor);
  equation_modify ^= equation_xor;
  constant_modify ^= constant_xor;
}

void DenseSystem::swapEquations(uint64_t equation_id_1,
                                uint64_t equation_id_2) {
  size_t base_index_1 = equation_id_1 * (_blocks_per_equation + 2);
  size_t base_index_2 = equation_id_2 * (_blocks_per_equation + 2);

  for (uint64_t i = 0; i < _blocks_per_equation; ++i) {
    std::swap(_equations[base_index_1 + i], _equations[base_index_2 + i]);
  }

  std::swap(getConstant(equation_id_1), getConstant(equation_id_2));

  std::swap(getFirstVar(equation_id_1), getFirstVar(equation_id_2));
}

void DenseSystem::updateFirstVar(uint64_t equation_id) {
  BitArray equation = getEquation(equation_id);
  uint64_t &constant = getConstant(equation_id);
  uint64_t &first_var = getFirstVar(equation_id);

  // returns the first non-zero bit index in equation_id's equation
  std::optional<uint64_t> temp_first_var = equation.find();
  // the equation is all 0s
  if (!temp_first_var.has_value()) {
    if (constant) {
      // In this case, we have a linearly dependent row
      throw UnsolvableSystemException(
          "Can't find a 1 in the equation yet the constant is 1.");
    } else {
      // In this case, we have an identity row
      first_var = _solution_size;
      return;
    }
  }

  first_var = *temp_first_var;
}

std::string DenseSystem::str() {
  std::string output;

  for (uint32_t equation_id = 0; equation_id < _equations.size();
       equation_id++) {
    BitArray equation = getEquation(equation_id);
    uint64_t &constant = getConstant(equation_id);
    output += equation.str() + " | " + std::to_string(constant) +
              "(Equation [" + std::to_string(equation_id) + "])\n";
  }
  return output;
}

DenseSystemPtr sparseToDense(const SparseSystemPtr &sparse_system) {
  uint32_t num_variables = sparse_system->solutionSize();

  DenseSystemPtr dense_system =
      DenseSystem::make(num_variables, sparse_system->numEquations());

  std::vector<uint64_t> equation_ids = sparse_system->equationIds();
  for (auto equation_id : equation_ids) {
    const uint64_t *equation_ptr = sparse_system->getEquation(equation_id);
    uint64_t constant = equation_ptr[3];
    std::unordered_set<uint32_t> vars_to_add;
    for (const uint64_t *var_id = equation_ptr; var_id < equation_ptr + 3;
         ++var_id) {
      if (!vars_to_add.count(*var_id)) {
        vars_to_add.insert(*var_id);
      } else {
        vars_to_add.erase(*var_id);
      }
    }
    // Update weight and priority for de-duped variables.
    dense_system->addEquation(
        equation_id,
        std::vector<uint64_t>(vars_to_add.begin(), vars_to_add.end()),
        constant);
  }

  return dense_system;
}

} // namespace caramel