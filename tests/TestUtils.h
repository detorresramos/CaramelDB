#pragma once
#include <algorithm>
#include <gtest/gtest.h>
#include <iterator>
#include <random>
#include <set>
#include <src/Modulo2System.h>
#include <sstream>
#include <string>
#include <unordered_map>

namespace caramel::tests {

inline void verifySolution(const SparseSystemPtr &original_sparse_system,
                           const BitArrayPtr &solution) {
  DenseSystemPtr original_system = sparseToDense(original_sparse_system);
  for (auto equation_id : original_sparse_system->equationIds()) {
    auto [equation, constant] = original_system->getEquation(equation_id);
    ASSERT_EQ(BitArray::scalarProduct(equation, solution), constant)
      << "Equation " << equation_id << " has solution " << constant
      << " but solvePeeledFromDense obtained solution "
      << BitArray::scalarProduct(equation, solution);
  }
}

inline SparseSystemPtr genRandomSparseSystem(uint32_t num_equations,
                                             uint32_t num_variables) {
  auto sparse_system = SparseSystem::make(num_variables);

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint32_t> var_dist(0, num_variables - 1);
  std::uniform_int_distribution<uint32_t> const_dist(0, 1);

  for (uint32_t equation_id = 0; equation_id < num_equations; equation_id++) {

    std::vector<uint32_t> vars;
    for (int i = 0; i < 3; i++) { vars.push_back(var_dist(gen)); }
    uint32_t constant = const_dist(gen);

    sparse_system->addEquation(equation_id, vars, constant);
  }

  return sparse_system;
}

inline std::vector<uint32_t> genRandomVector(uint32_t size) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint32_t> dist(0, 100);

  std::vector<uint32_t> vector(size);
  for (size_t i = 0; i < size; ++i) {
    vector[i] = dist(gen);
  }

  return vector;
}

inline void verify_peeling_order(std::vector<uint32_t> &unpeeled,
    std::vector<uint32_t> &peeled, std::vector<uint32_t> &order,
    const SparseSystemPtr &sparse_system, std::vector<uint32_t> &equation_ids) {

  std::stringstream error_stream;

  // Verify that unpeeled and peeled are disjoint (equation cannot be both).
  std::set<uint32_t> unpeeled_set(unpeeled.begin(), unpeeled.end());
  std::set<uint32_t> peeled_set(peeled.begin(), peeled.end());

  std::vector<uint32_t> intersection;
  std::set_intersection(
    unpeeled_set.begin(), unpeeled_set.end(),
    peeled_set.begin(), peeled_set.end(),
    std::back_inserter(intersection));

  for (uint32_t x : intersection){
    error_stream << x << " ";
  }
  ASSERT_EQ(intersection.size(), 0)
    << intersection.size()<< " equation_ids are both peeled and unpeeled: "
    << error_stream.str();
  error_stream.str(std::string());


  // Verify that all equation_ids are either peeled or unpeeled.
  std::set<uint32_t> all_ids(equation_ids.begin(), equation_ids.end());
  std::set<uint32_t> peeled_and_unpeeled_ids;
  peeled_and_unpeeled_ids.insert(peeled.begin(), peeled.end());
  peeled_and_unpeeled_ids.insert(unpeeled.begin(), unpeeled.end());
  std::vector<uint32_t> difference;

  std::set_difference(
    all_ids.begin(), all_ids.end(),
    peeled_and_unpeeled_ids.begin(), peeled_and_unpeeled_ids.end(),
    std::back_inserter(difference));
  
  ASSERT_EQ(difference.size(), 0)
    << difference.size() << " equation_ids are neither peeled nor unpeeled.";

  // Reverse the solution order to get the peeling order.
  std::reverse(order.begin(), order.end());
  std::reverse(peeled.begin(), peeled.end());

  ASSERT_EQ(order.size(), peeled.size())
    << "Variable solving order has size " << peeled.size() << " while peeling"
    << " order has size " << order.size();

  // Create a graph to check against.
  std::unordered_map<uint32_t, std::vector<uint32_t>> variable_to_equations;
  for (uint32_t equation_id : equation_ids) {
    auto [participating_variables, _ignore_constant_] =
          sparse_system->getEquation(equation_id);
    for (uint32_t variable_id : participating_variables) {
      variable_to_equations[variable_id].push_back(equation_id);
    }
  }

  // Now verify the equation peeling order by actually peeling the graph.
  for (size_t i = 0; i < order.size(); i++){
    uint32_t variable_id = order[i];
    uint32_t equation_id = peeled[i];
    // 1. Hinge variable should participate in only one unpeeled equation.
    ASSERT_EQ(variable_to_equations[variable_id].size(), 1)
      << variable_id << " participates in more than one equation. ";
    // 2. The equation it is participating in should be equation_id.
    ASSERT_EQ(variable_to_equations[variable_id][0], equation_id)
      << variable_id << " participates in equation "
      << variable_to_equations[variable_id][0] << " not " << equation_id
      << " as expected.";
    // 3. Remove the peeled equation from the graph.
    auto [participating_variables, _ignore_constant_] =
          sparse_system->getEquation(equation_id);
    for (uint32_t participating_variable : participating_variables) {
      // Remove all of the places where this equation shows up.
      variable_to_equations[participating_variable].erase(
        std::remove(variable_to_equations[participating_variable].begin(),
                    variable_to_equations[participating_variable].end(),
                    equation_id),
        variable_to_equations[participating_variable].end());
    }
  }

  // Check there aren't additional equations that could be peeled, but weren't.
  for (uint32_t equation_id : unpeeled) {
    auto [participating_variables, _ignore_constant_] = 
          sparse_system->getEquation(equation_id);
    for (uint32_t participating_variable : participating_variables) {
      int num_equations = variable_to_equations[participating_variable].size();
      ASSERT_GE(num_equations, 2)
        << "Equation " << equation_id << " can be peeled via variable "
        << participating_variable << ", which is in " << (num_equations - 1)
        << " other equations.";
    }
  }
}

} // namespace caramel::tests