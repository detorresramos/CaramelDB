#pragma once

#include "Filters.h"
#include "Rows.h"
#include "Spec.h"
#include <algorithm>
#include <cereal/types/vector.hpp>
#include <omp.h>
#include <src/construct/ConstructUtils.h>
#include <src/construct/FlatMap.h>

namespace caramel::build {

template <typename T> using Frequencies = FlatMap<T, uint64_t>;

template <typename T> struct ColumnCounts {
  Frequencies<T> freq;
  uint64_t count = 0; // values seen (Packed: rows seen)

  void merge(const ColumnCounts &other) {
    count += other.count;
    freq.reserve(freq.size() + other.freq.size());
    other.freq.forEach([&](const T &value, uint64_t f) { freq[value] += f; });
  }

  // Ties go to the smaller value so every worker picks the same MCV.
  std::pair<T, uint64_t> mostCommon() const {
    std::pair<T, uint64_t> best{T{}, 0};
    freq.forEach([&](const T &value, uint64_t f) {
      if (f > best.second || (f == best.second && value < best.first)) {
        best = {value, f};
      }
    });
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

// Merges maps into one. Outside a parallel region, large inputs scatter by
// hash into P disjoint partitions, merge those in parallel, then concatenate.
template <typename T>
Frequencies<T> mergeFrequencies(std::vector<Frequencies<T> *> parts) {
  size_t total = 0;
  for (auto *p : parts) {
    total += p->size();
  }
  if (parts.size() == 1 || total < (1u << 16) || omp_in_parallel()) {
    Frequencies<T> out = std::move(*parts[0]);
    for (size_t i = 1; i < parts.size(); i++) {
      out.reserve(out.size() + parts[i]->size());
      parts[i]->forEach([&](const T &v, uint64_t f) { out[v] += f; });
    }
    return out;
  }
  const size_t B = parts.size();
  const uint32_t P = static_cast<uint32_t>(omp_get_max_threads());
  std::vector<std::vector<std::vector<std::pair<T, uint64_t>>>> scattered(
      B, std::vector<std::vector<std::pair<T, uint64_t>>>(P));
#pragma omp parallel for schedule(dynamic, 1) default(none) shared(parts, scattered, B, P)
  for (size_t b = 0; b < B; b++) {
    parts[b]->forEach([&](const T &v, uint64_t f) {
      scattered[b][(Frequencies<T>::hash(v) >> 32) % P].emplace_back(v, f);
    });
    *parts[b] = {};
  }
  std::vector<Frequencies<T>> shards(P);
#pragma omp parallel for schedule(dynamic, 1) default(none) shared(scattered, shards, B, P)
  for (uint32_t p = 0; p < P; p++) {
    for (size_t b = 0; b < B; b++) {
      for (const auto &[v, f] : scattered[b][p]) {
        shards[p][v] += f;
      }
      scattered[b][p] = {};
    }
  }
  Frequencies<T> out;
  out.reserve(total);
  for (auto &shard : shards) {
    shard.forEach([&](const T &v, uint64_t f) { out[v] = f; });
    shard = {};
  }
  return out;
}

} // namespace detail

// Histograms every column over the rows a build will encode. With `filters`,
// a filtered column only counts the keys its filter passes (exceptions
// plus false positives), which is exactly what its codebook must cover.
template <typename T>
Counts<T> countValues(const BuildSpec &spec, const Signatures &sigs, const Rows<T> &rows,
             const Filters<T> *filters = nullptr) {
  const bool packed = spec.encoding == Encoding::Packed;
  const uint64_t n = rows.numRows();
  const uint32_t C = packed ? 1 : rows.numCols();
  if (sigs.size() != n) {
    throw std::invalid_argument("Keys and values must have the same length.");
  }

  std::vector<std::vector<uint64_t>> masks;
  if (filters && !filters->empty()) {
    masks = filters->passMasks(sigs);
  }
  auto active = [&](uint64_t r, uint32_t c) {
    if (!rows.present(r, c)) {
      return false;
    }
    int32_t f = filters ? filters->of(c) : -1;
    return f < 0 || maskBit(masks[f], r);
  };

  // One pass over the rows in B blocks, each counting into its own map per
  // column; then the blocks' maps merge per column.
  const uint32_t B = static_cast<uint32_t>(omp_get_max_threads());
  std::vector<std::vector<Frequencies<T>>> local(B, std::vector<Frequencies<T>>(C));
  std::vector<std::vector<uint64_t>> local_counts(B, std::vector<uint64_t>(C, 0));

#pragma omp parallel for schedule(static, 1) default(none)                     \
    shared(rows, local, local_counts, active, n, C, B, packed)
  for (uint32_t block = 0; block < B; block++) {
    auto &mine = local[block];
    auto &counts = local_counts[block];
    for (uint64_t r = n * block / B, end = n * (block + 1) / B; r < end; r++) {
      if (packed) {
        counts[0]++;
        for (uint32_t k = 0; k < rows.length(r); k++) {
          ++mine[0][rows.at(r, k)];
        }
        continue;
      }
      for (uint32_t c = 0; c < C; c++) {
        if (active(r, c)) {
          counts[c]++;
          ++mine[c][rows.at(r, c)];
        }
      }
    }
  }

  Counts<T> out;
  out.num_rows = n;
  out.num_cols = rows.numCols();
  out.ragged = rows.isRagged();
  out.cols.resize(C);
  // Columns merge in parallel; a few high-cardinality columns parallelize
  // inside mergeFrequencies instead.
  const bool nested = C * 4 <= B;
#pragma omp parallel for schedule(dynamic, 1) default(none)                    \
    shared(out, local, local_counts, C, B) if (!nested)
  for (uint32_t c = 0; c < C; c++) {
    std::vector<Frequencies<T> *> parts;
    for (uint32_t block = 0; block < B; block++) {
      parts.push_back(&local[block][c]);
      out.cols[c].count += local_counts[block][c];
    }
    out.cols[c].freq = detail::mergeFrequencies(std::move(parts));
  }
  return out;
}

} // namespace caramel::build
