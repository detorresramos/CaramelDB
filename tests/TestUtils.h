#pragma once
#include <algorithm>
#include <cmath>
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
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint32_t> var_dist(0, num_variables - 1);
  std::uniform_int_distribution<uint32_t> const_dist(0, 1);

  std::vector<std::vector<uint32_t>> equations;
  std::vector<uint32_t> constants;

  for (uint32_t equation_id = 0; equation_id < num_equations; equation_id++) {

    std::vector<uint32_t> vars;
    for (int i = 0; i < 3; i++) {
      vars.push_back(var_dist(gen));
    }
    uint32_t constant = const_dist(gen);
    equations.push_back(vars);
    constants.push_back(constant);
  }

  auto sparse_system = SparseSystem::make(std::move(equations),
                                          std::move(constants), num_variables);

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

inline std::vector<uint32_t> genRandomMatrix(size_t num_rows, size_t num_cols) {
  std::random_device rd;
  std::mt19937 gen(rd());
  uint32_t min_value = 0;
  uint32_t max_value = 100;
  std::uniform_int_distribution<uint32_t> dist(0, 100);

  size_t size = num_rows * num_cols;
  std::vector<uint32_t> vector(size);
  for (size_t row = 0; row < num_rows; row++) {
    for (size_t col = 0; col < num_cols; col++) {
      size_t index = row * num_cols + col;
      vector[index] = dist(gen);
    }
    // Ensure no duplicate values in the rows of the generated matrix.
    for (size_t col = 0; col < num_cols; col++) {
      size_t index = row * num_cols + col;
      int num_repeats = 0;
      for (size_t col_next = 0; col_next < num_cols; col_next++) {
        size_t index_next = row * num_cols + col_next;
        if (vector[index] == vector[index_next]){
          num_repeats++;
          vector[index_next] += max_value * num_repeats;
        }
      }
    }
  }

  return vector;
}

inline void verify_peeling_order(std::vector<uint32_t> &unpeeled,
                                 std::vector<uint32_t> &peeled,
                                 std::vector<uint32_t> &order,
                                 const SparseSystemPtr &sparse_system,
                                 std::vector<uint32_t> &equation_ids) {

  std::stringstream error_stream;

  // Verify that unpeeled and peeled are disjoint (equation cannot be both).
  std::set<uint32_t> unpeeled_set(unpeeled.begin(), unpeeled.end());
  std::set<uint32_t> peeled_set(peeled.begin(), peeled.end());

  std::vector<uint32_t> intersection;
  std::set_intersection(unpeeled_set.begin(), unpeeled_set.end(),
                        peeled_set.begin(), peeled_set.end(),
                        std::back_inserter(intersection));

  for (uint32_t x : intersection) {
    error_stream << x << " ";
  }
  ASSERT_EQ(intersection.size(), 0)
      << intersection.size()
      << " equation_ids are both peeled and unpeeled: " << error_stream.str();
  error_stream.str(std::string());

  // Verify that all equation_ids are either peeled or unpeeled.
  std::set<uint32_t> all_ids(equation_ids.begin(), equation_ids.end());
  std::set<uint32_t> peeled_and_unpeeled_ids;
  peeled_and_unpeeled_ids.insert(peeled.begin(), peeled.end());
  peeled_and_unpeeled_ids.insert(unpeeled.begin(), unpeeled.end());
  std::vector<uint32_t> difference;

  std::set_difference(
      all_ids.begin(), all_ids.end(), peeled_and_unpeeled_ids.begin(),
      peeled_and_unpeeled_ids.end(), std::back_inserter(difference));

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
  for (size_t i = 0; i < order.size(); i++) {
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

inline void verifyValidPermutation(const std::vector<uint32_t> &original,
                                  const std::vector<uint32_t> &permutation,
                                  size_t num_rows, size_t num_cols) {
  // Checks that the permutation is a valid transformation of the original.
  // Assumes vectors are in row-major order.
  ASSERT_EQ(original.size(), permutation.size())
      << "Permuted data does not have the same size as the original.";
  for (size_t row = 0; row < num_rows; row++) {
    for (size_t col = 0; col < num_cols; col++) {
      bool found_value = false;
      size_t original_location = row * num_cols + col;
      for (size_t col_p = 0; col_p < num_cols; col_p++) {
        size_t permutation_location = row * num_cols + col_p;
        if (original[original_location] == permutation[permutation_location]) {
          found_value = true;
        }
      }
      ASSERT_EQ(found_value, true)
          << "The value " << original[original_location] << " from row "<< row
          << " and column " << col << " in the original matrix is not present"
          << " in row " << row << " of the permuted matrix.";
    }
  }
}

inline float computeColumnEntropy(const std::vector<uint32_t> &matrix,
                                  size_t num_rows, size_t num_cols) {
  // Compute the empirical zero-order entropy.
  float entropy = 0;
  for (size_t col = 0; col < num_cols; col++) {
    float column_entropy = 0;
    std::unordered_map<uint32_t, int> frequency_map;
    for (size_t row = 0; row < num_rows; row++) {
      size_t index = row * num_cols + col;
      auto value = matrix[index];
      ++frequency_map[value];
    }
    for (const auto & [value, counts]: frequency_map) {
      float p = static_cast<float>(counts) / num_rows;
      column_entropy += -1.0 * p * std::log2(p);
    }
    entropy += column_entropy;
  }
  return entropy;
}

} // namespace caramel::tests