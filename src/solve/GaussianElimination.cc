#include "BitArray.h"
#include "Modulo2System.h"
#include "Solve.h"
#include <vector>

namespace caramel {

BitArray
gaussianElimination(DenseSystem &dense_system,
                    const std::vector<uint32_t> &relevant_equation_ids) {
  std::unordered_map<uint32_t, uint32_t> first_vars;
  for (uint32_t equation_id : relevant_equation_ids) {
    first_vars[equation_id] = dense_system.getFirstVar(equation_id);
  }

  uint32_t num_equations = relevant_equation_ids.size();
  for (uint32_t top_index = 0; top_index < num_equations - 1; top_index++) {
    for (uint32_t bot_index = top_index + 1; bot_index < num_equations;
         bot_index++) {
      uint32_t top_eq_id = relevant_equation_ids[top_index];
      uint32_t bot_eq_id = relevant_equation_ids[bot_index];

      if (first_vars[top_eq_id] == first_vars[bot_eq_id]) {
        // Since both virst vars are equal we'd like to eliminate one of them
        // via xor. The leading var in the top equation is above the leading var
        // in the bot equation so eliminate this variable from the bot equation.
        dense_system.xorEquations(bot_eq_id, top_eq_id);

        if (dense_system.isUnsolvable(top_eq_id)) {
          throw UnsolvableSystemException(
              "Equation " + std::to_string(top_eq_id) +
              " has all coefficients = 0 but constant is 1.");
        }

        first_vars[bot_eq_id] = dense_system.getFirstVar(bot_eq_id);
      }

      if (first_vars[top_eq_id] > first_vars[bot_eq_id]) {
        dense_system.swapEquations(top_eq_id, bot_eq_id);
        auto temp_first_vars = first_vars[top_eq_id];
        first_vars[top_eq_id] = first_vars[bot_eq_id];
        first_vars[bot_eq_id] = temp_first_vars;
      }
    }
  }

  uint32_t solution_size = dense_system.solutionSize();
  BitArray solution = BitArray(solution_size);
  solution.setall(0);
  for (uint32_t i = relevant_equation_ids.size(); i >= 0; i--) {
    uint32_t equation_id = relevant_equation_ids[i];
    auto [equation, constant] = dense_system.getEquation(equation_id);
    if (dense_system.isIdentity(equation_id)) {
      continue;
    }
    solution[first_vars[equation_id]] =
        np.bitwise_xor(constant, scalarProduct(equation, solution));
  }

  return solution;
}

} // namespace caramel