#include "LazyGaussianElimination.h"
#include <iostream>
#include <numeric>
#include <tuple>
#include <unordered_set>

namespace caramel {

std::tuple<std::unordered_map<uint32_t, std::vector<uint32_t>>,
           std::vector<uint32_t>, std::vector<uint32_t>, DenseSystemPtr>
constructDenseSystem(SparseSystemPtr &sparse_system,
                     const std::vector<uint32_t> &equation_ids) {
  uint32_t num_equations = sparse_system->numEquations();
  uint32_t num_variables = sparse_system->solutionSize();

  // The weight is the number of sparse equations containing variable_id.
  std::vector<uint32_t> variable_weight(num_variables, 0);

  // The equation priority is the number of idle variables in equation_id.
  std::vector<uint32_t> equation_priority(num_equations, 0);

  DenseSystemPtr dense_system = std::make_shared<DenseSystem>(num_variables);

  std::unordered_map<uint32_t, std::vector<uint32_t>> var_to_equations;
  for (uint32_t equation_id : equation_ids) {
    auto [participating_vars, constant] =
        sparse_system->getEquation(equation_id);
    // We should only add a variable to the equation in the dense system if
    // it appears an odd number of times. This is because we compute output
    // as XOR(solution[hash_1], solution[hash_2] ...). If hash_1 = hash_2 =
    // variable_id, then XOR(solution[hash_1], solution[hash_2]) = 0 and
    // the variable_id did not actually participate in the solution.
    std::unordered_set<uint32_t> vars_to_add;
    for (uint32_t variable_id : participating_vars) {
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
    for (uint32_t variable_id : vars_to_add) {
      variable_weight[variable_id]++;
      equation_priority[equation_id]++;
      var_to_equations[variable_id].push_back(equation_id);
    }
  }

  return {var_to_equations, equation_priority, variable_weight, dense_system};
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

std::vector<uint32_t>
countsortVariableIds(const std::vector<uint32_t> &variable_weight,
                     uint32_t num_variables, uint32_t num_equations) {
  // Sorts in ascending weight order in O(num_variables + max_weight) time.
  std::vector<uint32_t> sorted_variable_ids(num_variables);
  std::iota(std::begin(sorted_variable_ids), std::end(sorted_variable_ids), 0);
  std::vector<uint32_t> counts(num_equations + 1, 0);
  for (uint32_t variable_id = 0; variable_id < num_variables; variable_id++) {
    counts[variable_weight[variable_id]]++;
  }
  counts = cumsum(counts);
  for (int variable_id = num_variables - 1; variable_id >= 0; variable_id--) {
    uint32_t count_idx = variable_weight[variable_id];
    counts[count_idx]--;
    sorted_variable_ids[counts[count_idx]] = variable_id;
  }
  return sorted_variable_ids;
}

std::tuple<std::vector<uint32_t>, std::vector<uint32_t>, std::vector<uint32_t>,
           DenseSystemPtr>
lazyGaussianElimination(SparseSystemPtr &sparse_system,
                        const std::vector<uint32_t> &equation_ids) {
  uint32_t num_equations = sparse_system->numEquations();
  uint32_t num_variables = sparse_system->solutionSize();

  auto [var_to_equations, equation_priority, variable_weight, dense_system] =
      constructDenseSystem(sparse_system, equation_ids);

  // List of sparse equations with priority 0 or 1. Probably needs a re-name.
  std::vector<uint32_t> sparse_equation_ids;
  for (uint32_t id : equation_ids) {
    if (equation_priority.at(id) <= 1) {
      sparse_equation_ids.push_back(id);
    }
  }

  // List of dense equations with entirely active variables.
  std::vector<uint32_t> dense_equation_ids;
  // Equations that define a solved variable in terms of active variables.
  std::vector<uint32_t> solved_equation_ids;
  std::vector<uint32_t> solved_variable_ids;
  // List of currently-idle variables. Starts as a bit vector of all 1's, and
  // is filled in with 0s as variables become non-idle.
  BitArrayPtr idle_variable_indicator = BitArray::make(num_variables);
  idle_variable_indicator->setAll();

  // Sorted list of variable ids, in descending weight order.
  std::vector<uint32_t> sorted_variable_ids =
      countsortVariableIds(variable_weight, num_variables, num_equations);

  uint32_t num_active_variables = 0;
  uint32_t num_remaining_equations = equation_ids.size();

  while (num_remaining_equations > 0) {
    if (sparse_equation_ids.empty()) {
      // If there are no sparse equations with priority 0 or 1, then
      // we make another variable active and see if this status changes.
      uint32_t variable_id = sorted_variable_ids.back();
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
      for (uint32_t equation_id : var_to_equations[variable_id]) {
        equation_priority[equation_id] -= 1;
        if (equation_priority[equation_id] == 1) {
          sparse_equation_ids.push_back(equation_id);
        }
      }
    } else {
      // There is at least one sparse equation with priority 0 or 1.
      num_remaining_equations--;
      uint32_t equation_id = sparse_equation_ids.back();
      sparse_equation_ids.pop_back();
      if (equation_priority.at(equation_id) == 0) {
        auto [equation, constant] = dense_system->getEquation(equation_id);
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
      } else if (equation_priority.at(equation_id) == 1) {
        // If there is only 1 idle variable, the equation is solved.
        // We need to find the pivot - the variable_id of the only
        // remaining idle variable in the equation.
        auto [equation, constnat] = dense_system->getEquation(equation_id);
        uint32_t variable_id = *(*equation & *idle_variable_indicator)
                                    .find(); // TODO handle optional case?

        solved_variable_ids.push_back(variable_id);
        solved_equation_ids.push_back(equation_id);
        // By making the weight 0, we will skip this variable_id in the
        // future when looking for new active variables.
        variable_weight[variable_id] = 0;
        // Remove this variable from all other equations.
        for (uint32_t other_equation_id : var_to_equations[variable_id]) {
          if (other_equation_id != equation_id) {
            equation_priority[other_equation_id] -= 1;
            if (equation_priority[other_equation_id] == 1) {
              sparse_equation_ids.push_back(other_equation_id);
            } else if (equation_priority[other_equation_id] == 0) {
              // Check if solvable or identity?
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

} // namespace caramel