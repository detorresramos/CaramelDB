#pragma once

#include <unordered_map>
#include <vector>
#include <algorithm>

#include <iostream>

namespace caramel {

// Note: To allow for in-place modification, we need to have the call
// signature match the arguments. So, we need to specialize this template
// for np.int64, np.uint64, np.int32, np.uint32, etc and call the correct one.
// See SO post: https://stackoverflow.com/q/54793539

template <typename T>
void entropyPermutation(T* M, int num_rows, int num_cols) {
  using RowList = std::vector<int>;
  // 1. Build a map from column number to all eligible rows in that column.
  // Invariant: eligible_rows[c] contains all unassigned rows in column c.
  std::vector<RowList> eligible_rows(num_cols);
  for (int col = 0; col < num_cols; col++) {
    eligible_rows[col].resize(num_rows);
    std::iota(eligible_rows[col].begin(), eligible_rows[col].end(), 0);
  }

  // 2. Build a map from vocabulary value to rows containing that value.
  // Invariant: val_to_row[v] contains all rows where v can be relocated.
  std::unordered_map<T, RowList> val_to_row;  // value -> (rows).
  for (int row = 0; row < num_rows; row++) {
    for (int col = 0; col < num_cols; col++) {
      T value = M[row*num_cols + col];
      val_to_row[value].push_back(row);
    }
  }
  // Keep the rows sorted (required for fast set intersections).
  for (auto & [value, rows]: val_to_row) {
    std::sort(rows.begin(), rows.end());
  }

  // 4. Build a map from frequency of occurrence to list of values.
  // Invariant: frequency_map[k] contains all values with "k" relocatable rows.
  // To save memory, the table only has to be as large as the largest frequency.
  auto max_location = std::max_element(
      val_to_row.begin(), val_to_row.end(),
      [] (const std::pair<T, RowList>& a, const std::pair<T, RowList>& b)
          -> bool {return a.second.size() < b.second.size(); } );
  int max_frequency = max_location->second.size();
  std::vector<std::vector<T>> frequency_map(max_frequency+1);
  for (const auto & [value, rows]: val_to_row) {
    int frequency = rows.size();
    frequency_map[frequency].push_back(value);
  }

  // 5. Iteratively assign values to matrix locations until we cover all of M.
  int num_to_assign = num_rows * num_cols;
  while (num_to_assign > 0){
    // 5a. Remove the singleton values, since any location is optimal for them.
    num_to_assign = num_to_assign - frequency_map[1].size();
    frequency_map[1].clear();
    // 5b. Find the (v,c) combination that relocates the maximum number of rows
    // that contain a not-yet-assigned value v and are unassigned in column c.
    T best_value;
    RowList best_rows;
    int best_size = 0, best_col = 0;
    bool found_optimal = false;  // Used for early-termination heuristics.
    for (int frequency = max_frequency; frequency > 1; frequency--) {
      // Early terminate if no remaining frequency can possibly do better.
      found_optimal |= (frequency <= best_size);
      if (found_optimal) { break; }
      // Iterate through values, starting with the ones that occur most often.
      for (const T & value: frequency_map[frequency]) {
        for (int c = 0; c < num_cols; c++){
          RowList intersection;
          std::set_intersection(
              eligible_rows[c].begin(), eligible_rows[c].end(),
              val_to_row[value].begin(), val_to_row[value].end(),
              std::back_inserter(intersection));
          int new_size = intersection.size();
          if (new_size > best_size) {
            best_size = new_size;
            best_value = value;
            best_rows = intersection;  // TODO: Use std::move if not optimized.
            best_col = c;
          }
          // Eary terminate if we can group all items (chunk size = frequency)
          // because no subsequent (v,c) combination can do better.
          if (new_size == frequency) {
            found_optimal = true;
            break;  // Breaks only out of the argmax_(v,c) loop.
          }
        }
      }
    }

    // 5c. Perform the swap for all of the items.
    if (best_rows.size() == 0) { continue; }  // Shouldn't occur.
    for (const int & row: best_rows) {
      // Swap so that M[row, best_column] = best_value. Assumes no duplicates.
      T* row_location = M + (row*num_cols);
      T* p_value = std::find(row_location, row_location + num_cols, best_value);
      *p_value = row_location[best_col];
      row_location[best_col] = best_value;
      num_to_assign--;
    }
    // 5d. Maintain the loop invariants.
    RowList difference;
    std::set_difference(
        val_to_row[best_value].begin(), val_to_row[best_value].end(),
        best_rows.begin(), best_rows.end(),
        std::back_inserter(difference));
    RowList remaining_rows;
    std::set_difference(
      eligible_rows[best_col].begin(), eligible_rows[best_col].end(),
      best_rows.begin(), best_rows.end(),
      std::back_inserter(remaining_rows));
    // Invariant: frequency_map[k] contains all values with "k" relocatable rows
    int prev_freq = val_to_row[best_value].size();
    int curr_freq = difference.size();
    auto values_start = frequency_map[prev_freq].begin();
    auto values_end = frequency_map[prev_freq].end();
    auto value_it = std::find(values_start, values_end, best_value);
    if (value_it != values_end) {  // Remove value from frequency_map[previous].
      int index = value_it - values_start;
      frequency_map[prev_freq][index] = frequency_map[prev_freq].back();
      frequency_map[prev_freq].pop_back();
    }
    if (curr_freq > 0) {  // Add value to frequency_map[current].
      frequency_map[curr_freq].push_back(best_value);
    }
    // Invariant: val_to_row[v] contains all rows where v can be relocated.
    // If we were unable to assign all of the rows, there may be some left over.
    val_to_row[best_value] = difference;  // TODO: Try to std::move.
    // Invariant: eligible_rows[c] contains all unassigned rows in column c.
    eligible_rows[best_col] = remaining_rows;
  }
}

} // namespace caramel