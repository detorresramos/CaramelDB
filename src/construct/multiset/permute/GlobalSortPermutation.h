#pragma once

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>
#include <vector>

namespace caramel {

// Core ragged implementation: each row has a (potentially different) length.
// `row_offsets` has num_rows+1 entries; row i spans
// M[row_offsets[i] .. row_offsets[i+1]).
// `max_cols` is the maximum row length (number of column buckets for scoring).
template <typename T>
void globalSortPermutationRagged(T *M, const int *row_offsets, int num_rows,
                                 int max_cols,
                                 int refinement_iterations = 5) {
  // Phase 1: Global Frequency Sort
  int total = row_offsets[num_rows];
  std::unordered_map<T, int> global_counts;
  for (int i = 0; i < total; i++) {
    global_counts[M[i]]++;
  }

#pragma omp parallel for default(none)                                         \
    shared(M, global_counts, row_offsets, num_rows)
  for (int row = 0; row < num_rows; row++) {
    T *row_start = M + row_offsets[row];
    int row_len = row_offsets[row + 1] - row_offsets[row];
    std::sort(row_start, row_start + row_len, [&](const T &a, const T &b) {
      int ca = global_counts.at(a);
      int cb = global_counts.at(b);
      if (ca != cb)
        return ca > cb;
      return a < b;
    });
  }

  // Phase 2: Iterative Refinement (Hill Climbing)
  // Repeatedly build a scoreboard of item-column affinities, then greedily
  // reassign items within each row to their preferred columns.
  struct ColScore {
    float score;
    int col_idx;
    bool operator>(const ColScore &other) const { return score > other.score; }
  };

  struct PQItem {
    float score;
    int item_idx;
    int pref_idx;
    bool operator<(const PQItem &other) const { return score < other.score; }
  };

  for (int iter = 0; iter < refinement_iterations; iter++) {
    // Build per-column frequency maps. Parallelized by column: each thread
    // owns a disjoint subset of `counts` entries, so no synchronization needed.
    std::vector<std::unordered_map<T, int>> counts(max_cols);
#pragma omp parallel for schedule(static) default(none)                        \
    shared(counts, M, row_offsets, num_rows, max_cols)
    for (int col = 0; col < max_cols; col++) {
      auto &col_map = counts[col];
      for (int row = 0; row < num_rows; row++) {
        int off = row_offsets[row];
        int row_len = row_offsets[row + 1] - off;
        if (col < row_len) {
          col_map[M[off + col]]++;
        }
      }
    }

    // Pre-populate value_prefs entries serially so the subsequent parallel
    // fill phase only does read-only lookups on the map structure.
    std::unordered_map<T, std::vector<ColScore>> value_prefs;
    value_prefs.reserve(global_counts.size());
    std::vector<T> unique_values;
    unique_values.reserve(global_counts.size());
    for (const auto &[value, _] : global_counts) {
      value_prefs[value].resize(max_cols);
      unique_values.push_back(value);
    }

    // Parallel fill + sort: each thread owns a disjoint subset of values.
#pragma omp parallel for schedule(static) default(none)                        \
    shared(unique_values, value_prefs, counts, max_cols)
    for (size_t i = 0; i < unique_values.size(); i++) {
      const T value = unique_values[i];
      auto &prefs = value_prefs.at(value);
      for (int col = 0; col < max_cols; col++) {
        auto it = counts[col].find(value);
        int count = (it != counts[col].end()) ? it->second : 0;
        float score =
            (count > 0) ? std::log(static_cast<float>(count) + 1.0f) : 0.0f;
        prefs[col] = {score, col};
      }
      std::sort(prefs.begin(), prefs.end(), std::greater<ColScore>());
    }

#pragma omp parallel for default(none)                                         \
    shared(M, value_prefs, row_offsets, num_rows)
    for (int row = 0; row < num_rows; row++) {
      T *row_start = M + row_offsets[row];
      int row_len = row_offsets[row + 1] - row_offsets[row];

      std::priority_queue<PQItem> pq;
      std::vector<bool> col_used(row_len, false);
      std::vector<T> new_row(row_len);

      for (int item_idx = 0; item_idx < row_len; item_idx++) {
        const auto &prefs = value_prefs.at(row_start[item_idx]);
        for (int p = 0; p < static_cast<int>(prefs.size()); p++) {
          if (prefs[p].col_idx < row_len) {
            pq.push({prefs[p].score, item_idx, p});
            break;
          }
        }
      }

      int assigned = 0;
      while (assigned < row_len && !pq.empty()) {
        PQItem top = pq.top();
        pq.pop();

        T val = row_start[top.item_idx];
        const auto &prefs = value_prefs.at(val);
        int col_idx = prefs[top.pref_idx].col_idx;

        if (col_idx < row_len && !col_used[col_idx]) {
          col_used[col_idx] = true;
          new_row[col_idx] = val;
          assigned++;
        } else {
          for (int next = top.pref_idx + 1;
               next < static_cast<int>(prefs.size()); next++) {
            if (prefs[next].col_idx < row_len) {
              pq.push({prefs[next].score, top.item_idx, next});
              break;
            }
          }
        }
      }

      std::copy(new_row.begin(), new_row.end(), row_start);
    }
  }
}

// Fixed-length convenience wrapper: all rows have the same number of columns.
template <typename T>
void globalSortPermutation(T *M, int num_rows, int num_cols,
                           int refinement_iterations = 5) {
  std::vector<int> row_offsets(num_rows + 1);
  for (int i = 0; i <= num_rows; i++) {
    row_offsets[i] = i * num_cols;
  }
  globalSortPermutationRagged<T>(M, row_offsets.data(), num_rows, num_cols,
                                 refinement_iterations);
}

} // namespace caramel
