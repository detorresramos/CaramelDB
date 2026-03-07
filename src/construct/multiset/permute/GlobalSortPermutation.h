#pragma once

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>
#include <vector>

namespace caramel {

template <typename T>
void globalSortPermutation(T *M, int num_rows, int num_cols,
                           int refinement_iterations = 5) {
  // Phase 1: Global Frequency Sort
  // Sort each row so the most globally frequent items land in leftmost columns.
  std::unordered_map<T, int> global_counts;
  for (int i = 0; i < num_rows * num_cols; i++) {
    global_counts[M[i]]++;
  }

#pragma omp parallel for default(none) shared(M, global_counts, num_rows, num_cols)
  for (int row = 0; row < num_rows; row++) {
    T *row_start = M + row * num_cols;
    std::sort(row_start, row_start + num_cols, [&](const T &a, const T &b) {
      int ca = global_counts.at(a);
      int cb = global_counts.at(b);
      if (ca != cb) return ca > cb;
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
    bool operator<(const PQItem &other) const {
      return score < other.score;
    }
  };

  for (int iter = 0; iter < refinement_iterations; iter++) {
    // Build counts: how many times each value appears in each column.
    std::vector<std::unordered_map<T, int>> counts(num_cols);
    for (int row = 0; row < num_rows; row++) {
      for (int col = 0; col < num_cols; col++) {
        counts[col][M[row * num_cols + col]]++;
      }
    }

    // For each unique value, build a sorted preference list of columns.
    std::unordered_map<T, std::vector<ColScore>> value_prefs;
    for (const auto &[value, _] : global_counts) {
      auto &prefs = value_prefs[value];
      prefs.reserve(num_cols);
      for (int col = 0; col < num_cols; col++) {
        auto it = counts[col].find(value);
        int count = (it != counts[col].end()) ? it->second : 0;
        float score =
            (count > 0) ? std::log(static_cast<float>(count) + 1.0f) : 0.0f;
        prefs.push_back({score, col});
      }
      std::sort(prefs.begin(), prefs.end(), std::greater<ColScore>());
    }

    // Greedy assignment: for each row, assign items to their best available
    // column using a priority queue to resolve conflicts.
#pragma omp parallel for default(none) shared(M, value_prefs, num_rows, num_cols)
    for (int row = 0; row < num_rows; row++) {
      T *row_start = M + row * num_cols;

      std::priority_queue<PQItem> pq;
      std::vector<bool> col_used(num_cols, false);
      std::vector<T> new_row(num_cols);

      for (int item_idx = 0; item_idx < num_cols; item_idx++) {
        const auto &prefs = value_prefs.at(row_start[item_idx]);
        if (!prefs.empty()) {
          pq.push({prefs[0].score, item_idx, 0});
        }
      }

      int assigned = 0;
      while (assigned < num_cols && !pq.empty()) {
        PQItem top = pq.top();
        pq.pop();

        T val = row_start[top.item_idx];
        const auto &prefs = value_prefs.at(val);
        int col_idx = prefs[top.pref_idx].col_idx;

        if (!col_used[col_idx]) {
          col_used[col_idx] = true;
          new_row[col_idx] = val;
          assigned++;
        } else {
          int next = top.pref_idx + 1;
          if (next < static_cast<int>(prefs.size())) {
            pq.push({prefs[next].score, top.item_idx, next});
          }
        }
      }

      std::copy(new_row.begin(), new_row.end(), row_start);
    }
  }
}

} // namespace caramel
