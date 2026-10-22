#pragma once

#include "Filters.h"
#include "Rows.h"
#include "Spec.h"
#include <algorithm>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/vector.hpp>
#include <omp.h>
#include <src/construct/ConstructUtils.h>
#include <unordered_map>

namespace caramel::build {

template <typename T> struct ColumnCounts {
  std::unordered_map<T, uint64_t> freq;
  uint64_t count = 0; // values seen (Packed: rows seen)

  void merge(const ColumnCounts &other) {
    count += other.count;
    for (const auto &[value, f] : other.freq) {
      freq[value] += f;
    }
  }

  // Ties go to the smaller value so every worker picks the same MCV.
  std::pair<T, uint64_t> mostCommon() const {
    std::pair<T, uint64_t> best{T{}, 0};
    for (const auto &[value, f] : freq) {
      if (f > best.second || (f == best.second && value < best.first)) {
        best = {value, f};
      }
    }
    return best;
  }

  template <class Archive> void serialize(Archive &archive) {
    archive(freq, count);
  }
};

// Value histograms of a set of rows. Mergeable: the counts of a union of
// chunks is the merge of the chunks' counts, in any order.
template <typename T> struct Counts {
  uint64_t num_rows = 0;
  uint32_t num_cols = 0; // ragged: the longest row seen
  bool ragged = false;
  std::vector<ColumnCounts<T>> cols; // Packed: one entry over every value

  void merge(const Counts &other) {
    if (ragged != other.ragged || (!ragged && num_cols != other.num_cols)) {
      throw std::invalid_argument("Cannot merge counts of different layouts.");
    }
    num_rows += other.num_rows;
    num_cols = std::max(num_cols, other.num_cols);
    if (cols.size() < other.cols.size()) {
      cols.resize(other.cols.size());
    }
    for (size_t c = 0; c < other.cols.size(); c++) {
      cols[c].merge(other.cols[c]);
    }
  }

  template <class Archive> void serialize(Archive &archive) {
    archive(num_rows, num_cols, ragged, cols);
  }
};

namespace detail {

template <typename T> inline uint32_t valuePartition(const T &v, uint32_t parts) {
  uint64_t h = std::hash<T>{}(v) * 0x9E3779B97F4A7C15ULL;
  return static_cast<uint32_t>((h >> 32) % parts);
}

} // namespace detail

// Histograms every column over the rows a build will encode. With `filters`,
// a filtered column only counts the keys its filter passes (exceptions
// plus false positives), which is exactly what its codebook must cover.
template <typename T>
Counts<T> countValues(const BuildSpec &spec, const Keys &keys, const Rows<T> &rows,
             const Filters<T> *filters = nullptr) {
  const bool packed = spec.encoding == Encoding::Packed;
  const uint64_t n = rows.numRows();
  const uint32_t C = packed ? 1 : rows.numCols();
  if (keys.size() != n) {
    throw std::invalid_argument("Keys and values must have the same length.");
  }

  std::vector<std::vector<uint64_t>> masks;
  if (filters && !filters->empty()) {
    masks = filters->passMasks(keys);
  }
  auto active = [&](uint64_t r, uint32_t c) {
    if (!rows.present(r, c)) {
      return false;
    }
    int32_t f = filters ? filters->of(c) : -1;
    return f < 0 || maskBit(masks[f], r);
  };

  // A column is split into P value partitions so a single high-entropy column
  // still histograms on every thread, and the partials stay disjoint.
  const uint32_t threads = static_cast<uint32_t>(omp_get_max_threads());
  const uint32_t P = std::clamp<uint32_t>(threads / std::max<uint32_t>(C, 1), 1, 16);
  std::vector<std::unordered_map<T, uint64_t>> partial(static_cast<size_t>(C) * P);
  std::vector<uint64_t> col_counts(C, 0);

#pragma omp parallel for schedule(dynamic, 1) default(none)                    \
    shared(rows, partial, col_counts, active, n, C, P, packed)
  for (size_t task = 0; task < static_cast<size_t>(C) * P; task++) {
    const uint32_t c = task / P;
    const uint32_t p = task % P;
    auto &local = partial[task];
    uint64_t count = 0;
    for (uint64_t r = 0; r < n; r++) {
      if (packed) {
        count++;
        for (uint32_t k = 0; k < rows.length(r); k++) {
          const T &v = rows.at(r, k);
          if (P == 1 || detail::valuePartition(v, P) == p) {
            ++local[v];
          }
        }
      } else if (active(r, c)) {
        count++;
        const T &v = rows.at(r, c);
        if (P == 1 || detail::valuePartition(v, P) == p) {
          ++local[v];
        }
      }
    }
    if (p == 0) {
      col_counts[c] = count;
    }
  }

  Counts<T> out;
  out.num_rows = n;
  out.num_cols = rows.numCols();
  out.ragged = rows.isRagged();
  out.cols.resize(C);
#pragma omp parallel for schedule(dynamic, 1) default(none)                    \
    shared(out, partial, col_counts, C, P)
  for (uint32_t c = 0; c < C; c++) {
    auto &freq = out.cols[c].freq;
    size_t total = 0;
    for (uint32_t p = 0; p < P; p++) {
      total += partial[c * P + p].size();
    }
    freq.reserve(total);
    for (uint32_t p = 0; p < P; p++) {
      auto &part = partial[c * P + p];
      freq.insert(part.begin(), part.end());
      part = {};
    }
    out.cols[c].count = col_counts[c];
  }
  return out;
}

} // namespace caramel::build
