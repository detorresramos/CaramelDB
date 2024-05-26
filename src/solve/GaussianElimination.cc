#include "GaussianElimination.h"

namespace caramel {

BitArrayPtr
gaussianElimination(const DenseSystemPtr &dense_system,
                    const std::vector<uint64_t> &relevant_equation_ids) {
  for (uint64_t equation_id : relevant_equation_ids) {
    dense_system->updateFirstVar(equation_id);
  }

  int num_equations = relevant_equation_ids.size();
  for (int top_index = 0; top_index < num_equations - 1; top_index++) {
    for (int bot_index = top_index + 1; bot_index < num_equations;
         bot_index++) {
      uint64_t top_eq_id = relevant_equation_ids[top_index];
      auto &[top_eq, top_const, top_first_var] =
          dense_system->getEquation(top_eq_id);

      uint64_t bot_eq_id = relevant_equation_ids[bot_index];
      auto &[bot_eq, bot_const, bot_first_var] =
          dense_system->getEquation(bot_eq_id);

      if (top_first_var == bot_first_var) {
        // Since both virst vars are equal we'd like to eliminate one of them
        // via xor. The leading var in the top equation is above the leading var
        // in the bot equation so eliminate this variable from the bot equation.
        dense_system->xorEquations(bot_eq_id, top_eq_id);

        if (dense_system->isUnsolvable(top_eq_id)) {
          throw UnsolvableSystemException(
              "Equation " + std::to_string(top_eq_id) +
              " has all coefficients = 0 but constant is 1.");
        }

        dense_system->updateFirstVar(bot_eq_id);
      }

      if (top_first_var > bot_first_var) {
        dense_system->swapEquations(top_eq_id, bot_eq_id);
      }
    }
  }

  uint64_t solution_size = dense_system->solutionSize();
  BitArrayPtr solution = BitArray::make(solution_size);
  for (int i = relevant_equation_ids.size() - 1; i >= 0; i--) {
    uint64_t equation_id = relevant_equation_ids[i];
    if (dense_system->isIdentity(equation_id)) {
      continue;
    }

    auto &[equation, constant, first_var] =
        dense_system->getEquation(equation_id);
    if (constant ^ BitArray::scalarProduct(equation, solution)) {
      solution->setBit(first_var);
    }
  }

  return solution;
}

} // namespace caramel