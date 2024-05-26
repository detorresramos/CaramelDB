#include "LazyGaussianElimination.h"
#include <cassert>
#include <numeric>
#include <tuple>
#include <unordered_set>

namespace caramel {

std::tuple<std::unordered_map<uint64_t, std::vector<uint64_t>>,
           std::unordered_map<uint64_t, uint32_t>, std::vector<uint32_t>,
           DenseSystemPtr>
constructDenseSystem(const SparseSystemPtr &sparse_system,
                     const std::vector<uint64_t> &equation_ids) {

  uint64_t num_variables = sparse_system->solutionSize();

  // The weight is the number of sparse equations containing variable_id.
  std::vector<uint32_t> variable_weight(num_variables, 0);

  // The equation priority is the number of idle variables in equation_id.
  std::unordered_map<uint64_t, uint32_t> equation_priority;
  equation_priority.reserve(equation_ids.size());

  DenseSystemPtr dense_system =
      DenseSystem::make(num_variables, sparse_system->numEquations());

  std::unordered_set<uint64_t> vars_to_add;
  std::unordered_map<uint64_t, std::vector<uint64_t>> var_to_equations;
  var_to_equations.reserve(num_variables);
  for (uint64_t equation_id : equation_ids) {
    auto [participating_vars, constant] =
        sparse_system->getEquation(equation_id);
    // We should only add a variable to the equation in the dense system if
    // it appears an odd number of times. This is because we compute output
    // as XOR(solution[hash_1], solution[hash_2] ...). If hash_1 = hash_2 =
    // variable_id, then XOR(solution[hash_1], solution[hash_2]) = 0 and
    // the variable_id did not actually participate in the solution.
    for (uint64_t variable_id : participating_vars) {
      auto [_, inserted] = vars_to_add.insert(variable_id);
      if (!inserted) {
        vars_to_add.erase(variable_id);
      }
    }
    dense_system->addEquation(equation_id, vars_to_add, constant);
    // Update weight and priority for de-duped variables.
    for (uint64_t variable_id : vars_to_add) {
      variable_weight[variable_id]++;
      var_to_equations[variable_id].push_back(equation_id);
    }
    equation_priority[equation_id] += vars_to_add.size();
    vars_to_add.clear();
  }

  return {std::move(var_to_equations), std::move(equation_priority),
          std::move(variable_weight), dense_system};
}

std::vector<uint32_t> cumsum(const std::vector<uint32_t> &input) {
  std::vector<uint32_t> output(input.size());
  uint32_t cumulated = 0;
  for (size_t i = 0; i < input.size(); ++i) {
    cumulated += input[i];
    output[i] = cumulated;
  }
  return output;
}

std::vector<uint64_t>
countsortVariableIds(const std::vector<uint32_t> &variable_weight,
                     uint64_t num_variables, uint64_t num_equations) {
  // Sorts in ascending weight order in O(num_variables + max_weight) time.
  std::vector<uint64_t> sorted_variable_ids(num_variables);
  std::iota(std::begin(sorted_variable_ids), std::end(sorted_variable_ids), 0);
  std::vector<uint32_t> counts(num_equations + 1, 0);
  for (uint64_t variable_id = 0; variable_id < num_variables; variable_id++) {
    counts[variable_weight[variable_id]]++;
  }
  counts = cumsum(counts);
  for (uint64_t variable_id = num_variables - 1; variable_id > 0;
       variable_id--) {
    uint32_t count_idx = variable_weight[variable_id];
    counts[count_idx]--;
    sorted_variable_ids[counts[count_idx]] = variable_id;
  }
  uint32_t count_idx = variable_weight[0];
  counts[count_idx]--;
  sorted_variable_ids[counts[count_idx]] = 0;
  return sorted_variable_ids;
}

std::tuple<std::vector<uint64_t>, std::vector<uint64_t>, std::vector<uint64_t>,
           DenseSystemPtr>
lazyGaussianElimination(const SparseSystemPtr &sparse_system,
                        const std::vector<uint64_t> &equation_ids) {
  uint64_t num_equations = sparse_system->numEquations();
  uint64_t num_variables = sparse_system->solutionSize();

  auto [var_to_equations, equation_priority, variable_weight, dense_system] =
      constructDenseSystem(sparse_system, equation_ids);

  // List of sparse equations with priority 0 or 1. Probably needs a re-name.
  std::vector<uint64_t> sparse_equation_ids;
  for (uint64_t id : equation_ids) {
    if (equation_priority.find(id)->second <= 1) {
      sparse_equation_ids.push_back(id);
    }
  }

  // List of dense equations with entirely active variables.
  std::vector<uint64_t> dense_equation_ids;
  // Equations that define a solved variable in terms of active variables.
  std::vector<uint64_t> solved_equation_ids;
  std::vector<uint64_t> solved_variable_ids;
  // List of currently-idle variables. Starts as a bit vector of all 1's, and
  // is filled in with 0s as variables become non-idle.
  BitArrayPtr idle_variable_indicator = BitArray::make(num_variables);
  idle_variable_indicator->setAll();

  // Sorted list of variable ids, in descending weight order.
  std::vector<uint64_t> sorted_variable_ids =
      countsortVariableIds(variable_weight, num_variables, num_equations);

  uint32_t num_active_variables = 0;
  uint64_t num_remaining_equations = equation_ids.size();

  while (num_remaining_equations > 0) {
    if (sparse_equation_ids.empty()) {
      // If there are no sparse equations with priority 0 or 1, then
      // we make another variable active and see if this status changes.
      uint64_t variable_id = sorted_variable_ids.back();
      sorted_variable_ids.pop_back();
      // Skip variables with weight = 0 (these are already solved).
      while (variable_weight[variable_id] == 0) {
        variable_id = sorted_variable_ids.back();
        sorted_variable_ids.pop_back();
      }
      // Mark variable as no longer idle
      idle_variable_indicator->clearBit(variable_id);
      num_active_variables++;

      // By marking this variable as active, we must update priorities.
      for (uint64_t equation_id : var_to_equations[variable_id]) {
        equation_priority[equation_id] -= 1;
        if (equation_priority[equation_id] == 1) {
          sparse_equation_ids.push_back(equation_id);
        }
      }
    } else {
      // There is at least one sparse equation with priority 0 or 1.
      num_remaining_equations--;
      uint64_t equation_id = sparse_equation_ids.back();
      sparse_equation_ids.pop_back();
      uint32_t priority = equation_priority.find(equation_id)->second;
      if (priority == 0) {
        auto &[equation, constant, _] = dense_system->getEquation(equation_id);
        bool equation_is_nonempty = equation->any();
        if (equation_is_nonempty) {
          // Since priority is 0, all variables are active.
          dense_equation_ids.push_back(equation_id);
        } else if (constant != 0) {
          throw UnsolvableSystemException(
              "Equation " + std::to_string(equation_id) +
              " has all coefficients = 0 but constant is 1.");
        }
        // The remaining case corresponds to an identity equation
        //(which is empty, but so is the output so it's fine).
      } else if (priority == 1) {
        // If there is only 1 idle variable, the equation is solved.
        // We need to find the pivot - the variable_id of the only
        // remaining idle variable in the equation.
        auto &[equation, constant, _] = dense_system->getEquation(equation_id);
        uint64_t variable_id = *(*equation & *idle_variable_indicator)
                                    .find(); // TODO handle optional case?

        solved_variable_ids.push_back(variable_id);
        solved_equation_ids.push_back(equation_id);
        // By making the weight 0, we will skip this variable_id in the
        // future when looking for new active variables.
        variable_weight[variable_id] = 0;
        // Remove this variable from all other equations.
        for (uint64_t other_equation_id : var_to_equations[variable_id]) {
          if (other_equation_id != equation_id) {
            equation_priority[other_equation_id] -= 1;
            if (equation_priority[other_equation_id] == 1) {
              sparse_equation_ids.push_back(other_equation_id);
            } else if (equation_priority[other_equation_id] == 0) {
              // Check if solvable or identity?
              // TODO check if we should throw UnsolvableSystemException here.
            }
            dense_system->xorEquations(other_equation_id, equation_id);
          }
        }
      }
    }
  }

  return {dense_equation_ids, solved_equation_ids, solved_variable_ids,
          dense_system};
}

BitArrayPtr solveLazyFromDense(const std::vector<uint64_t> &solved_ids,
                               const std::vector<uint64_t> &solved_vars,
                               const DenseSystemPtr &dense_system,
                               const BitArrayPtr &dense_solution) {
  // Solve the lazy gaussian elimination variables using the solutions to the
  // dense linear system (that were found using regular gaussian elimination).
  // dense_solution: The solution to dense_system, from GaussianElimiation.

  assert(solved_ids.size() == solved_vars.size());
  for (uint32_t i = 0; i < solved_ids.size(); i++) {
    uint32_t equation_id = solved_ids[i];
    uint32_t variable_id = solved_vars[i];
    // The only non - dense variable that still needs to be solved is
    // variable_id(by the invariants of the lazy gaussian elimination)
    // The solution is zero at this index, so the bit to set is just
    // the constant XOR < equation_coefficients, solution_so_far>
    auto &[equation, constant, _] = dense_system->getEquation(equation_id);
    // TODO: The mod-2 might be less efficient than alternatives (this is a
    // holdover from the Python implementation where bitwise ops were hard).
    // If X = BitArray::scalarProduct, check if we can replace X % 2 with X & 1.
    uint32_t value =
        constant ^ BitArray::scalarProduct(equation, dense_solution) % 2;
    value = 1 & value;
    if (value) {
      dense_solution->setBit(variable_id);
    }
  }

  return dense_solution;
}

} // namespace caramel