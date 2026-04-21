#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>
#include <unordered_map>
#include <vector>

namespace caramel {

// Combiner for the OMP user-defined reduction used in Phase 1 count. Merges
// one thread-local frequency map into another by summing values for shared
// keys. Tree-reduced across threads by OpenMP.
template <typename K, typename V>
inline void mergeCountsMap(std::unordered_map<K, V> &dst,
                           const std::unordered_map<K, V> &src) {
  for (const auto &p : src) {
    dst[p.first] += p.second;
  }
}

// Core ragged implementation: each row has a (potentially different) length.
// `row_offsets` has num_rows+1 entries; row i spans
// M[row_offsets[i] .. row_offsets[i+1]).
// `max_cols` is the maximum row length (number of column buckets for scoring).
//
// All internal indices/counts are int64_t. Any cumulative cell position
// (anything derived from row_offsets[]) would silently overflow a 32-bit
// int for N*M > 2^31 (e.g. 50M rows x 100 cols); using int64_t everywhere
// avoids the class of narrowing bugs where one missed site corrupts an
// offset and dereferences garbage.
template <typename T>
void globalSortPermutationRagged(T *M, const int64_t *row_offsets,
                                 int64_t num_rows, int64_t max_cols,
                                 int refinement_iterations = 5) {
  // Phase 1: Global Frequency Sort
  int64_t total = row_offsets[num_rows];
  std::unordered_map<T, int64_t> global_counts;
#pragma omp declare reduction(countsmerge : std::unordered_map<T, int64_t> :   \
    mergeCountsMap(omp_out, omp_in))
#pragma omp parallel for reduction(countsmerge : global_counts)
  for (int64_t i = 0; i < total; i++) {
    global_counts[M[i]]++;
  }

  // Chunked scheduling amortizes OMP dispatch overhead for short per-row
  // sorts (~100 elements typical).
#pragma omp parallel for schedule(static, 64) default(none)                    \
    shared(M, global_counts, row_offsets, num_rows)
  for (int64_t row = 0; row < num_rows; row++) {
    T *row_start = M + row_offsets[row];
    int64_t row_len = row_offsets[row + 1] - row_offsets[row];
    std::sort(row_start, row_start + row_len, [&](const T &a, const T &b) {
      int64_t ca = global_counts.at(a);
      int64_t cb = global_counts.at(b);
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
    int64_t col_idx;
    bool operator>(const ColScore &other) const { return score > other.score; }
  };

  struct PQItem {
    float score;
    int64_t item_idx;
    int64_t pref_idx;
    bool operator<(const PQItem &other) const { return score < other.score; }
  };

  for (int iter = 0; iter < refinement_iterations; iter++) {
    // Build per-column frequency maps. Parallelized by column: each thread
    // owns a disjoint subset of `counts` entries, so no synchronization needed.
    std::vector<std::unordered_map<T, int64_t>> counts(max_cols);
#pragma omp parallel for schedule(static) default(none)                        \
    shared(counts, M, row_offsets, num_rows, max_cols)
    for (int64_t col = 0; col < max_cols; col++) {
      auto &col_map = counts[col];
      for (int64_t row = 0; row < num_rows; row++) {
        int64_t off = row_offsets[row];
        int64_t row_len = row_offsets[row + 1] - off;
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
      for (int64_t col = 0; col < max_cols; col++) {
        auto it = counts[col].find(value);
        int64_t count = (it != counts[col].end()) ? it->second : 0;
        float score =
            (count > 0) ? std::log(static_cast<float>(count) + 1.0f) : 0.0f;
        prefs[col] = {score, col};
      }
      std::sort(prefs.begin(), prefs.end(), std::greater<ColScore>());
    }

#pragma omp parallel for default(none)                                         \
    shared(M, value_prefs, row_offsets, num_rows)
    for (int64_t row = 0; row < num_rows; row++) {
      T *row_start = M + row_offsets[row];
      int64_t row_len = row_offsets[row + 1] - row_offsets[row];

      std::priority_queue<PQItem> pq;
      std::vector<bool> col_used(row_len, false);
      std::vector<T> new_row(row_len);
      // Cache each item's prefs pointer once per row so the PQ loop doesn't
      // re-run value_prefs.at() on every pop.
      std::vector<const std::vector<ColScore> *> prefs_ptrs(row_len);
      for (int64_t i = 0; i < row_len; i++) {
        prefs_ptrs[i] = &value_prefs.at(row_start[i]);
      }

      for (int64_t item_idx = 0; item_idx < row_len; item_idx++) {
        const auto &prefs = *prefs_ptrs[item_idx];
        for (int64_t p = 0; p < static_cast<int64_t>(prefs.size()); p++) {
          if (prefs[p].col_idx < row_len) {
            pq.push({prefs[p].score, item_idx, p});
            break;
          }
        }
      }

      int64_t assigned = 0;
      while (assigned < row_len && !pq.empty()) {
        PQItem top = pq.top();
        pq.pop();

        T val = row_start[top.item_idx];
        const auto &prefs = *prefs_ptrs[top.item_idx];
        int64_t col_idx = prefs[top.pref_idx].col_idx;

        if (col_idx < row_len && !col_used[col_idx]) {
          col_used[col_idx] = true;
          new_row[col_idx] = val;
          assigned++;
        } else {
          for (int64_t next = top.pref_idx + 1;
               next < static_cast<int64_t>(prefs.size()); next++) {
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
void globalSortPermutation(T *M, int64_t num_rows, int64_t num_cols,
                           int refinement_iterations = 5) {
  std::vector<int64_t> row_offsets(num_rows + 1);
  for (int64_t i = 0; i <= num_rows; i++) {
    row_offsets[i] = i * num_cols;
  }
  globalSortPermutationRagged<T>(M, row_offsets.data(), num_rows, num_cols,
                                 refinement_iterations);
}

} // namespace caramel
