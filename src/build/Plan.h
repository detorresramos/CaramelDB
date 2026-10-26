#pragma once

#include "Filters.h"
#include "Kernel.h"
#include "Counts.h"
#include <cereal/types/memory.hpp>
#include <iostream>
#include <limits>
#include <src/construct/CsfCodebook.h>

namespace caramel::build {

// Columns encoded together: same key set (same filter, same row presence) and
// one bucket count, so they share bucket ids and an arena.
struct GroupPlan {
  std::vector<uint32_t> columns;
  int32_t filter = -1;
  uint32_t num_buckets = 0;

  template <class Archive> void serialize(Archive &archive) {
    archive(columns, filter, num_buckets);
  }
};

// A contiguous run of one group's buckets: the unit of solve work.
struct Partition {
  uint32_t group = 0;
  uint32_t begin = 0;
  uint32_t end = 0;

  uint32_t size() const { return end - begin; }

  template <class Archive> void serialize(Archive &archive) {
    archive(group, begin, end);
  }
};

// Everything a worker needs to split and solve: small next to the data, and
// identical on every worker, so no worker ever has to talk to another.
template <typename T> struct Plan {
  BuildSpec spec;
  uint32_t num_cols = 0;
  bool ragged = false;
  // The codebook choice, resolved (spec.codebook may be Auto).
  bool shared_codebook = false;
  // Per column (Packed: one). Null when the column encodes nothing. With a
  // shared codebook every entry points at the same codebook.
  std::vector<std::shared_ptr<CsfCodebook<T>>> codebooks;
  Filters<T> filters;
  std::vector<GroupPlan> groups;
  T stop_symbol{}; // Packed: terminates a row's code stream

  bool packed() const { return spec.encoding == Encoding::Packed; }

  // Splits every group into at most `count` partitions of near-equal size.
  std::vector<Partition> partitions(uint32_t count) const {
    std::vector<Partition> out;
    for (uint32_t g = 0; g < groups.size(); g++) {
      const uint64_t nb = groups[g].num_buckets;
      const uint64_t k = std::min<uint64_t>(std::max<uint32_t>(count, 1), nb);
      for (uint64_t i = 0; i < k; i++) {
        out.push_back({g, static_cast<uint32_t>(nb * i / k),
                       static_cast<uint32_t>(nb * (i + 1) / k)});
      }
    }
    return out;
  }

  // The value -> code maps are construction-only. A local build drops a
  // group's maps once it is solved to keep peak memory down.
  void releaseCodedicts(uint32_t group) {
    if (shared_codebook) {
      return;
    }
    for (uint32_t c : groups[group].columns) {
      if (codebooks[c]) {
        codebooks[c]->codedict = CodeDict<T>();
      }
    }
  }

  template <class Archive> void save(Archive &archive) const {
    archive(spec, num_cols, ragged, shared_codebook, codebooks, filters, groups,
            stop_symbol);
  }

  template <class Archive> void load(Archive &archive) {
    archive(spec, num_cols, ragged, shared_codebook, codebooks, filters, groups,
            stop_symbol);
    for (auto &cb : codebooks) {
      if (cb && cb->codedict.empty()) {
        cb->buildCodedict();
      }
    }
  }
};

namespace detail {

template <typename T>
uint64_t codedBits(const Frequencies<T> &freq, const CsfCodebook<T> &codebook) {
  uint64_t bits = 0;
  freq.forEach([&](const T &value, uint64_t f) {
    bits += f * codebook.codedict.at(value).length;
  });
  return bits;
}

// Serialized size of a codebook's symbol table.
template <typename T> uint64_t tableBits(const CsfCodebook<T> &codebook) {
  return 8 * (sizeof(T) * codebook.ordered_symbols.size() +
              sizeof(uint32_t) * (codebook.code_length_counts.size() + 1));
}

// Splits columns that share a key set into groups whose target bucket counts
// stay within 2x, so one high-entropy column does not force its neighbours to
// over-partition.
inline void appendBucketSubGroups(std::vector<GroupPlan> &groups,
                                  std::vector<uint32_t> columns, int32_t filter,
                                  const std::vector<uint64_t> &col_buckets) {
  constexpr uint64_t BUCKET_RATIO = 2;
  std::stable_sort(columns.begin(), columns.end(), [&](uint32_t a, uint32_t b) {
    return col_buckets[a] < col_buckets[b];
  });
  uint64_t sub_min = 0;
  bool open = false;
  for (uint32_t c : columns) {
    uint64_t nb = col_buckets[c];
    if (!open || nb > sub_min * BUCKET_RATIO) {
      groups.push_back({{}, filter, 0});
      sub_min = nb;
      open = true;
    }
    groups.back().columns.push_back(c);
    groups.back().num_buckets =
        std::max<uint32_t>(groups.back().num_buckets, static_cast<uint32_t>(nb));
  }
}

} // namespace detail

// Codebooks, bucket counts and groups from the counts of the rows to encode
// (with filters: the counts countValues() reports given those filters).
template <typename T>
Plan<T> makePlan(const BuildSpec &spec, const Counts<T> &counts,
                 Filters<T> filters = {}) {
  Plan<T> plan;
  plan.spec = spec;
  plan.num_cols = counts.num_cols;
  plan.ragged = counts.ragged;
  plan.filters = std::move(filters);
  if (plan.filters.column_filter.empty()) {
    plan.filters.column_filter.assign(counts.num_cols, -1);
  }
  const uint32_t C = static_cast<uint32_t>(counts.cols.size());
  std::vector<uint64_t> col_buckets(C, 0);

  if (plan.packed()) {
    plan.stop_symbol = std::numeric_limits<T>::max();
    auto freq = counts.cols[0].freq;
    if (freq.count(plan.stop_symbol)) {
      throw std::invalid_argument("Packed values must not contain the STOP "
                                  "symbol (the maximum value of the type).");
    }
    freq[plan.stop_symbol] = counts.cols[0].count;
    auto cb = std::make_shared<CsfCodebook<T>>(
        canonicalHuffmanFromFrequencies<T>(freq));
    col_buckets[0] = packedBucketCount(cb->coded_bits, counts.cols[0].count);
    plan.codebooks = {cb};
    plan.groups.push_back({{0}, -1, static_cast<uint32_t>(col_buckets[0])});
    return plan;
  }

  const bool try_own = spec.codebook != CodebookMode::Shared;
  const bool try_shared = spec.codebook == CodebookMode::Shared ||
                          (spec.codebook == CodebookMode::Auto && C > 1);
  std::shared_ptr<CsfCodebook<T>> shared;
  if (try_shared) {
    ColumnCounts<T> pooled;
    for (const auto &col : counts.cols) {
      pooled.merge(col);
    }
    if (pooled.count > 0) {
      shared = std::make_shared<CsfCodebook<T>>(
          canonicalHuffmanFromFrequencies<T>(pooled.freq));
    }
  }

  std::vector<std::shared_ptr<CsfCodebook<T>>> own(C);
  std::vector<uint64_t> own_bits(C, 0), shared_bits(C, 0);
#pragma omp parallel for schedule(dynamic, 1) default(none)                    \
    shared(counts, shared, own, own_bits, shared_bits, try_own, C)
  for (uint32_t c = 0; c < C; c++) {
    const auto &col = counts.cols[c];
    if (col.count == 0) {
      continue;
    }
    if (try_own) {
      own[c] = std::make_shared<CsfCodebook<T>>(
          canonicalHuffmanFromFrequencies<T>(col.freq));
      own_bits[c] = own[c]->coded_bits;
    }
    if (shared) {
      shared_bits[c] = detail::codedBits(col.freq, *shared);
    }
  }

  plan.shared_codebook = shared && spec.codebook == CodebookMode::Shared;
  if (shared && spec.codebook == CodebookMode::Auto) {
    // Solutions cost ribbon_overhead bits per code bit in either mode, so
    // the modes differ by code bits and by how many symbol tables are stored.
    double own_cost = 0, shared_cost = detail::tableBits(*shared);
    for (uint32_t c = 0; c < C; c++) {
      if (own[c]) {
        own_cost += spec.ribbon_overhead * own_bits[c] + detail::tableBits(*own[c]);
        shared_cost += spec.ribbon_overhead * shared_bits[c];
      }
    }
    plan.shared_codebook = shared_cost < own_cost;
    if (spec.verbose) {
      std::cout << "Codebook: " << (plan.shared_codebook ? "shared" : "per column")
                << " (estimated " << own_cost / 8 << " B per column, "
                << shared_cost / 8 << " B shared)" << std::endl;
    }
  }

  plan.codebooks.resize(C);
  for (uint32_t c = 0; c < C; c++) {
    if (counts.cols[c].count == 0) {
      continue;
    }
    plan.codebooks[c] = plan.shared_codebook ? shared : std::move(own[c]);
    col_buckets[c] = columnBucketCount(
        plan.shared_codebook ? shared_bits[c] : own_bits[c], counts.cols[c].count);
  }

  if (counts.ragged) {
    // Row presence differs per column, so every column is its own group.
    for (uint32_t c = 0; c < C; c++) {
      plan.groups.push_back({{c}, plan.filters.of(c),
                             static_cast<uint32_t>(col_buckets[c])});
    }
    return plan;
  }

  // Columns with the same filter (or none) share a key set.
  std::vector<int32_t> filter_order;
  for (uint32_t c = 0; c < C; c++) {
    int32_t f = plan.filters.of(c);
    if (std::find(filter_order.begin(), filter_order.end(), f) ==
        filter_order.end()) {
      filter_order.push_back(f);
    }
  }
  for (int32_t f : filter_order) {
    std::vector<uint32_t> columns;
    for (uint32_t c = 0; c < C; c++) {
      if (plan.filters.of(c) == f) {
        columns.push_back(c);
      }
    }
    detail::appendBucketSubGroups(plan.groups, std::move(columns), f,
                                  col_buckets);
  }
  return plan;
}

} // namespace caramel::build
