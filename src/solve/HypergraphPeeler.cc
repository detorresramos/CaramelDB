#include "HypergraphPeeler.h"
#include <algorithm>
#include <set>
#include <tuple>


namespace caramel {

std::tuple<std::vector<uint32_t>, std::vector<uint32_t>, std::vector<uint32_t>,
           SparseSystemPtr>
peelHypergraph(const SparseSystemPtr &sparse_system,
               const std::vector<uint64_t> &equation_ids) {
  uint64_t num_equations = sparse_system->numEquations();
  uint64_t num_variables = sparse_system->solutionSize();

  // Degree of a variable is the number of unpeeled equations that contain it.
  std::vector<uint32_t> degree(num_variables, 0);
  // equation_is_peeled[equation_id] = 1 if variable_id has been peeled.
  // TODO: Check that we use a lookup / space-efficient version of vector<bool>.
  std::vector<bool> equation_is_peeled(num_equations, 0);
  // Stores the XOR of edges (equations) each variable participates in.
  std::vector<uint32_t> equation_id_xors(num_variables, 0);

  for (uint64_t equation_id : equation_ids){
    auto [participating_variables, _ignore_constant_] = 
      sparse_system->getEquation(equation_id);

#ifdef DEBUG_HYPERGRAPH
    std::cout<<"Edge (equation) "<<equation_id<< ": <";
    for (uint32_t variable_id : participating_variables) {
      std::cout<<" "<<variable_id;
    } std::cout<<">"<<std::endl;
#endif

    for (uint32_t variable_id : participating_variables) {
      // Increment the degree for each vertex in the incident edge.
      degree[variable_id]++;
      // Add the edge to the XOR list corresponding to variable_id.
      equation_id_xors[variable_id] ^= equation_id;
    }
  }

#ifdef DEBUG_HYPERGRAPH
  std::cout << "Degree (var) :";
  for (uint32_t variable_id = 0; variable_id < num_variables; variable_id++) {
    std::cout << " [" << variable_id << ":" << degree[variable_id] << "]";
  } std::cout << std::endl;

  std::cout << "XORs (var) :";
  for (uint32_t variable_id = 0; variable_id < num_variables; variable_id++) {
    std::cout << " [" << variable_id << ":";
  } std::cout << equation_id_xors[variable_id] << "]" << std::endl;
#endif


  std::vector<uint32_t> vertex_stack;
  // This is different from the Python for efficiency reasons only. It is
  // allocated out here to avoid re-allocation / re-initialization in the loop.
  std::vector<uint32_t> vars_to_peel;

  for (uint32_t variable_id = 0; variable_id < num_variables; variable_id++){
    if (degree[variable_id] == 1) {
      // Then we should peel the only equation containing variable_id.
      vars_to_peel.clear();
      vars_to_peel.push_back(variable_id);
      uint32_t num_processed = 0;
      while (num_processed < vars_to_peel.size()){
        // The first trip through this inner loop, we peel the equation that
        // contains variable_id. Subsequent trips through the loop peel
        // equations that have become "freed up" by previous peeling steps.
        uint32_t var_to_peel = vars_to_peel[num_processed];
        num_processed++;
        // If degree is zero, then we've already peeled this equation.
        // If the degree is > 1, then we can't peel this equation anyway.
        if (degree[var_to_peel] != 1){
          continue;
        }
        vertex_stack.push_back(var_to_peel);
        // Because var_to_peel participates in only one unpeeled equation,
        // equation_id_xors contains that equation id (as all the other xor ops
        // have been undone).
        uint32_t peeled_equation_id = equation_id_xors[var_to_peel];
        equation_is_peeled[peeled_equation_id] = true;

#ifdef DEBUG_HYPERGRAPH
        std::cout << "Peeling variable " << var_to_peel << " in equation ";
        std::cout << peeled_equation_id << std::endl;
        std::cout << "Peeling order (so far):";
        for (uint32_t e : vertex_stack) {
          std::cout << " " << e;
        } std::cout << std::endl;
#endif

        // We must remove peeled_equation_id from equation_id_xors for the other
        // variables that participate in this equation.
        auto [vars_to_update, _ignore_constant_] =
          sparse_system->getEquation(peeled_equation_id);
        for (uint32_t var_to_update : vars_to_update) {
          // Since we peeled this equation, decrease the degree.
          degree[var_to_update]--;
          if (var_to_update != var_to_peel) {
            // If this isn't the variable we are currently peeling, remove it
            // from the XOR list (If it is, then doing the XOR is pointless as
            // it will just yield 0).
            equation_id_xors[var_to_update] ^= peeled_equation_id;
          }
        }
        // Iterate through the other variables involved in the peeled equation,
        // to see if any of them have been "freed up" by the peel.

        // Because of how the hashing construction works, vars_to_update may
        // contain duplicates. We de-dupe with a set to process it only once.
        std::set<uint32_t> set_to_update(
          vars_to_update.begin(), vars_to_update.end());
        for (uint32_t var_to_maybe_peel : set_to_update) {
          if (degree[var_to_maybe_peel] == 1) {
            vars_to_peel.push_back(var_to_maybe_peel);
          }
        }
      }
    }
  }

  std::vector<uint32_t> unpeeled_equation_ids;
  for (uint32_t equation_id : equation_ids) {
    if (!equation_is_peeled[equation_id]) {
      unpeeled_equation_ids.push_back(equation_id);
    }
  }

  // Variables, listed in the order they should be solved via back-sub.
  std::reverse(vertex_stack.begin(), vertex_stack.end());
  // Note: in following comments, let var_solution_order = vertex_stack.

  // The peeled_equation_ids array is ordered so that peeled_equation_ids[n] can
  // be used to solve for the variable with variable_id = var_solution_order[n].
  std::vector<uint32_t> peeled_equation_ids(vertex_stack.size(), 0);
  for (size_t i = 0; i < vertex_stack.size(); i++) {
    uint32_t var = vertex_stack[i];
    peeled_equation_ids[i] = equation_id_xors[var];
  }

#ifdef DEBUG_HYPERGRAPH
  std::cout << "Peeled " << peeled_equation_ids.size() << " of "; 
  std::cout << num_equations << "("; 
  std::cout << 100 * float(peeled_equation_ids.size()) / num_equations;
  std::cout << "%)." << std::endl;
#endif

  return {unpeeled_equation_ids, peeled_equation_ids, vertex_stack,
          sparse_system};
}


BitArrayPtr solvePeeledFromDense(const std::vector<uint32_t> &peeled_ids,
                                 const std::vector<uint32_t> &solution_order,
                                 const SparseSystemPtr &sparse_system,
                                 const BitArrayPtr &dense_solution) {
  // Solve the peeled hypergraph representation of the linear system using the
  // solution to the unpeelable 2-core of the system (dense_solution), from
  // either LazyGaussianElimination or from GaussianElimination.

  // peeled_ids and solution_order should be the same size. We omit the check
  // for performance (and also because this is covered by the unit tests).

  for (size_t i = 0; i < peeled_ids.size(); i++) {
    uint32_t equation_id = peeled_ids[i];
    uint32_t variable_id = solution_order[i];

    // Update dense_solution to include the solution to this variable.
    auto [participating_vars, constant] =
        sparse_system->getEquation(equation_id);
    bool accumulator = false;
    for (uint32_t participating_var : participating_vars) {
      accumulator ^= (*dense_solution)[participating_var];
    }
    accumulator ^= constant;
    if (accumulator) {
      dense_solution->setBit(variable_id);
    } else {
      dense_solution->clearBit(variable_id);
    }
  }
  return dense_solution;
}

} // namespace caramel