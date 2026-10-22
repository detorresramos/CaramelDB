#pragma once
#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <iterator>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>

namespace caramel::tests {

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
        if (vector[index] == vector[index_next]) {
          num_repeats++;
          vector[index_next] += max_value * num_repeats;
        }
      }
    }
  }

  return vector;
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
          << "The value " << original[original_location] << " from row " << row
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
    for (const auto &[value, counts] : frequency_map) {
      float p = static_cast<float>(counts) / num_rows;
      column_entropy += -1.0 * p * std::log2(p);
    }
    entropy += column_entropy;
  }
  return entropy;
}

} // namespace caramel::tests