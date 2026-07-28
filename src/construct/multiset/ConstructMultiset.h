#pragma once

#include "src/construct/Construct.h"
#include "src/construct/multiset/permute/EntropyPermutation.h"
#include "src/construct/multiset/permute/GlobalSortPermutation.h"
#include "src/construct/multiset/MultisetCsf.h"
#include "src/construct/multiset/MultisetConfig.h"
#include "src/construct/filter/FilterFactory.h"
#include "src/utils/Timer.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <omp.h>

namespace caramel {

// Converts column-major values to a row-major flat buffer, applies the given
// permutation function, then converts back.
template <typename T, typename PermFn>
void applyPermutation(std::vector<std::vector<T>> &values, PermFn fn) {
  size_t num_columns = values.size();
  size_t num_rows = values[0].size();

  std::vector<T> buf(num_rows * num_columns);
  for (size_t c = 0; c < num_columns; c++) {
    for (size_t r = 0; r < num_rows; r++) {
      buf[r * num_columns + c] = values[c][r];
    }
  }
  fn(buf.data(), num_rows, num_columns);
  for (size_t c = 0; c < num_columns; c++) {
    for (size_t r = 0; r < num_rows; r++) {
      values[c][r] = buf[r * num_columns + c];
    }
  }
}


template <typename T>
struct ColumnFilterInfo {
  PreFilterPtr<T> filter;
  T mcv{};
  std::vector<bool> is_minority_key;
};

// Builds per-MCV-group shared filters. Columns with the same per-column MCV
// share a single filter; columns with different MCVs get separate groups.
template <typename T>
std::vector<ColumnFilterInfo<T>>
buildGroupSharedFilters(const std::vector<std::string> &keys,
                        const std::vector<std::vector<T>> &values,
                        PreFilterConfigPtr filter_config,
                        bool verbose) {
  size_t num_columns = values.size();
  size_t num_keys = keys.size();

  // Compute each column's MCV
  std::vector<T> column_mcvs(num_columns);
  for (size_t i = 0; i < num_columns; i++) {
    std::unordered_map<T, size_t> freq;
    for (const auto &v : values[i]) {
      freq[v]++;
    }
    T best{};
    size_t best_count = 0;
    for (const auto &[val, count] : freq) {
      if (count > best_count) {
        best_count = count;
        best = val;
      }
    }
    column_mcvs[i] = best;
  }

  // Group columns by MCV value
  std::unordered_map<T, std::vector<size_t>> mcv_groups;
  for (size_t i = 0; i < num_columns; i++) {
    mcv_groups[column_mcvs[i]].push_back(i);
  }

  std::vector<ColumnFilterInfo<T>> result(num_columns);

  for (const auto &[mcv, col_indices] : mcv_groups) {
    // A key is minority for this group if ANY column in the group differs
    std::vector<bool> is_minority_key(num_keys, false);
    for (size_t k = 0; k < num_keys; k++) {
      for (size_t ci : col_indices) {
        if (values[ci][k] != mcv) {
          is_minority_key[k] = true;
          break;
        }
      }
    }

    // Find a sentinel value (any value != mcv) for synthetic filter input
    T sentinel = mcv;
    for (size_t ci : col_indices) {
      for (const auto &v : values[ci]) {
        if (v != mcv) {
          sentinel = v;
          goto found_sentinel;
        }
      }
    }
    found_sentinel:

    std::vector<T> synthetic_values(num_keys);
    for (size_t k = 0; k < num_keys; k++) {
      synthetic_values[k] = is_minority_key[k] ? sentinel : mcv;
    }

    auto actual_config = filter_config;
    if (std::dynamic_pointer_cast<AutoPreFilterConfig>(filter_config)) {
      actual_config = selectBestFilter<T>(synthetic_values);
    }

    if (actual_config) {
      auto filter = FilterFactory::makeFilter<T>(actual_config);
      std::vector<std::string> filtered_keys_out;
      std::vector<T> filtered_values_out;
      filter->apply(keys, synthetic_values, filtered_keys_out,
                    filtered_values_out, DELTA, verbose);

      for (size_t ci : col_indices) {
        result[ci] = {filter, mcv, is_minority_key};
      }
    }
  }

  return result;
}

template <typename T>
struct ColumnInputs {
  PreFilterPtr<T> filter;
  std::vector<std::string> keys;
  std::vector<T> values;
  std::optional<T> most_common_value;
};

// Resolves which keys/values a column's CSF needs to encode, depending on
// whether we're using a group filter, per-column filter, or no filter.
template <typename T>
ColumnInputs<T>
resolveColumnInputs(const std::vector<std::string> &all_keys,
                    const std::vector<T> &column_values,
                    const ColumnFilterInfo<T> *col_filter_info,
                    PreFilterConfigPtr filter_config,
                    bool verbose) {
  if (col_filter_info) {
    std::vector<std::string> keys;
    std::vector<T> values;
    keys.reserve(all_keys.size());
    values.reserve(all_keys.size());
    for (size_t k = 0; k < all_keys.size(); k++) {
      if (col_filter_info->filter->contains(all_keys[k])) {
        keys.push_back(all_keys[k]);
        values.push_back(column_values[k]);
      }
    }
    return {col_filter_info->filter, std::move(keys), std::move(values),
            col_filter_info->mcv};
  }

  if (filter_config) {
    auto actual_config = filter_config;
    if (std::dynamic_pointer_cast<AutoPreFilterConfig>(filter_config)) {
      actual_config = selectBestFilter<T>(column_values);
    }
    if (actual_config) {
      auto filter = FilterFactory::makeFilter<T>(actual_config);
      std::vector<std::string> keys;
      std::vector<T> values;
      filter->apply(all_keys, column_values, keys, values, DELTA, verbose);
      std::optional<T> mcv = filter->getMostCommonValue();
      return {filter, std::move(keys), std::move(values), mcv};
    }
  }

  return {nullptr, {}, {}, std::nullopt};
}

// Per-column resolved state used while building groups.
template <typename T>
struct ResolvedColumn {
  uint32_t col_index = 0;  // original output position
  PreFilterPtr<T> filter;
  std::optional<T> most_common_value;
  std::vector<std::string> keys;  // active keys (filtered or full)
  std::vector<T> values;          // active values (may be left empty for
                                  // no-filter columns, re-extracted on demand
                                  // from the row-major buffer to avoid holding
                                  // all M columns' values at once)
  std::shared_ptr<CsfCodebook<T>> codebook;
  uint64_t num_buckets = 0;       // precomputed so grouping/build never re-read
                                  // values just to size buckets
};

// Codebook for one column. A column whose keys were all filtered out has no
// active values to encode, so it gets a null codebook (and a zero-bucket
// degenerate group) rather than feeding canonicalHuffman an empty input.
template <typename T>
std::shared_ptr<CsfCodebook<T>>
columnCodebook(const std::vector<T> &values, bool shared,
               const std::shared_ptr<CsfCodebook<T>> &shared_cb) {
  if (shared) {
    return shared_cb;
  }
  if (values.empty()) {
    return nullptr;
  }
  return std::make_shared<CsfCodebook<T>>(canonicalHuffman<T>(values));
}

// Builds the interleaved arena in place, streaming one column at a time so peak
// memory stays ~1x: the whole bucket-major layout is sized directly from the
// shared key partition (no bucketed value copy needed to size), the arena is
// allocated once, then each column scatters its source values, solves every
// bucket into its precomputed word-aligned slot, and frees its values before
// the next column. At most O(threads) columns' value buckets and O(threads)
// solution BitArrays are ever resident -- never all M columns at once, and
// never a second full copy of the solution bits (the old solve-then-pack was,
// costing ~2x peak at scale). `part` is the shared key partition (identical
// across the group's columns); source_values_per_col[c] is consumed in place.
template <typename T>
void fillArena(
    typename MultisetCsf<T>::BucketArena &arena, const KeyPartition &part,
    std::vector<std::vector<T>> &source_values_per_col,
    const std::vector<std::shared_ptr<CsfCodebook<T>>> &codebooks,
    const std::vector<uint32_t> &col_indices, const T *data, size_t num_columns,
    bool codebooks_disposable, float DELTA) {
  const uint32_t M = static_cast<uint32_t>(source_values_per_col.size());
  const uint32_t num_buckets = part.num_buckets;
  const size_t cells = static_cast<size_t>(num_buckets) * M;
  arena.num_cols = M;
  arena.num_buckets = num_buckets;
  arena.per_col_bits.assign(cells, 0);
  arena.per_col_seeds.assign(cells, 0);
  arena.per_col_word_off.assign(cells, 0);

  // The codedict summation (one DRAM-bound lookup per value for high-entropy
  // data) is the single expensive sizing pass. Compute it once here, keep the
  // per-cell equation count, and hand it to the solver so it never re-sums.
  std::vector<uint64_t> per_cell_equations(cells, 0);
  const std::vector<uint32_t> &bucket_of_key = part.bucket_of_key;
  const size_t n = bucket_of_key.size();

  Timer size_timer;
  // Size pass: a subsystem's width is the coded bit-length of its bucket's
  // values, which we accumulate straight from each column's source values via
  // bucket_of_key -- no bucketed copy required. A column's values are either
  // provided (filtered columns) or re-extracted on demand from the row-major
  // `data` buffer (no-filter columns, so we never hold all M at once). Columns
  // are independent (each writes only its own cells idx = b*M + c), so
  // parallelize over them.
#pragma omp parallel for schedule(dynamic, 1) default(none)                    \
    shared(source_values_per_col, codebooks, arena, per_cell_equations,        \
           bucket_of_key, col_indices, data, num_columns, n, num_buckets, M,   \
           DELTA)
  for (uint32_t c = 0; c < M; c++) {
    std::vector<T> owned;
    const std::vector<T> *valsp = &source_values_per_col[c];
    if (valsp->empty() && n > 0) {
      owned.resize(n);
      uint32_t ci = col_indices[c];
      for (size_t r = 0; r < n; r++) {
        owned[r] = data[r * num_columns + ci];
      }
      valsp = &owned;
    }
    const auto &codedict = codebooks[c]->codedict;
    const auto &vals = *valsp;
    std::vector<uint64_t> eqs(num_buckets, 0);
    for (size_t i = 0; i < vals.size(); i++) {
      eqs[bucket_of_key[i]] += codedict.find(vals[i])->second.numBits();
    }
    for (uint32_t b = 0; b < num_buckets; b++) {
      size_t idx = static_cast<size_t>(b) * M + c;
      per_cell_equations[idx] = eqs[b];
      arena.per_col_bits[idx] =
          solutionBitsFromEquations(eqs[b], codebooks[c]->max_codelength, DELTA);
    }
  }
  uint64_t total_words = 0;
  for (size_t idx = 0; idx < cells; idx++) {
    arena.per_col_word_off[idx] = total_words;
    total_words += (arena.per_col_bits[idx] + 63u) / 64u;
  }
  // +1 trailing guard word: decode getbits() reads arr[w] and arr[w+1].
  arena.solution_bits.assign(total_words + 1, 0);

  const bool timing = std::getenv("CARAMEL_TIMING") != nullptr;
  double size_s = size_timer.seconds();
  Timer solve_timer;

  // Solve pass: one column at a time (parallel over columns). Each column
  // scatters its source values into buckets -- aligned with part.key_buckets,
  // since both preserve input order within a bucket -- solves each bucket into
  // its disjoint arena slot, then frees its values. Disjoint slots need no lock
  // (only exception capture does).
  uint64_t *out = arena.solution_bits.data();
  std::exception_ptr exception = nullptr;
#pragma omp parallel for schedule(dynamic, 1) default(none)                    \
    shared(source_values_per_col, codebooks, arena, out, exception,            \
           per_cell_equations, part, col_indices, data, num_columns, n,        \
           num_buckets, M, codebooks_disposable, DELTA)
  for (uint32_t c = 0; c < M; c++) {
    if (exception) {
      continue;
    }
    try {
      std::vector<T> owned;
      const std::vector<T> *valsp = &source_values_per_col[c];
      if (valsp->empty() && n > 0) {
        owned.resize(n);
        uint32_t ci = col_indices[c];
        for (size_t r = 0; r < n; r++) {
          owned[r] = data[r * num_columns + ci];
        }
        valsp = &owned;
      }
      std::vector<std::vector<T>> value_buckets = scatterValues<T>(*valsp, part);
      source_values_per_col[c] = std::vector<T>();
      owned = std::vector<T>();
      const auto &codedict = codebooks[c]->codedict;
      uint32_t max_cl = codebooks[c]->max_codelength;
      for (uint32_t b = 0; b < num_buckets; b++) {
        size_t idx = static_cast<size_t>(b) * M + c;
        auto [solution, seed] = constructAndSolveSubsystem<T>(
            part.key_buckets[b], value_buckets[b], codedict, max_cl, DELTA,
            per_cell_equations[idx]);
        assert(solution->numBits() == arena.per_col_bits[idx]);
        if (seed > UINT8_MAX) {
          throw std::runtime_error("Solve seed exceeded one byte.");
        }
        arena.per_col_seeds[idx] = static_cast<uint8_t>(seed);
        const uint64_t *src = solution->backingArrayPtr();
        std::copy(src, src + (arena.per_col_bits[idx] + 63u) / 64u,
                  out + arena.per_col_word_off[idx]);
      }
      // The value->code map is construction-only (queries decode via
      // ordered_symbols; codedict is never serialized). When these codebooks
      // aren't shared with other columns, drop it as soon as the column is
      // solved so the fat map shrinks during the solve -- this both lowers the
      // build peak and leaves the served index ~codedict-free.
      if (codebooks_disposable) {
        codebooks[c]->codedict = CodeDict<T>();
      }
    } catch (std::exception &) {
#pragma omp critical
      { exception = std::current_exception(); }
    }
  }
  if (exception) {
    std::rethrow_exception(exception);
  }
  if (timing) {
    uint64_t arena_bytes =
        static_cast<uint64_t>(arena.solution_bits.size()) * 8 +
        static_cast<uint64_t>(cells) * (4 + 4 + 8); // bits + seeds + word_off
    std::cerr << "[timing] fillArena M=" << M << " num_buckets=" << num_buckets
              << " cells=" << cells << " words=" << total_words
              << " arena=" << arena_bytes / 1e9 << "GB size_pass=" << size_s
              << "s solve_pass=" << solve_timer.seconds() << "s\n";
  }
}

// Runs partitionToBuckets once for a group of columns that share active keys,
// then solves each column's subsystems against that single hash store and
// packs all solutions into a flat interleaved arena.
template <typename T>
typename MultisetCsf<T>::Group
buildGroup(const std::vector<std::string> &active_keys,
           const std::vector<ResolvedColumn<T> *> &group_cols,
           bool shared_codebook, const T *data = nullptr,
           size_t num_columns = 0) {
  using GroupT = typename MultisetCsf<T>::Group;
  using GroupColumnT = typename MultisetCsf<T>::GroupColumn;
  GroupT group;

  // Group-level num_buckets = max across member columns (precomputed at resolve
  // time). This guarantees every column fits in the shared bucket layout.
  uint64_t num_buckets = 0;
  for (const auto *col : group_cols) {
    num_buckets = std::max(num_buckets, col->num_buckets);
  }

  if (num_buckets == 0 || active_keys.empty()) {
    // Degenerate group: all keys were filtered out for every column. Keep
    // zero-bucket arena; query returns the most-common value per column.
    group.hash_store_seed = 0;
    group.arena.num_cols = static_cast<uint32_t>(group_cols.size());
    group.arena.num_buckets = 0;
    group.columns.reserve(group_cols.size());
    for (const auto *col : group_cols) {
      GroupColumnT gc;
      gc.output_index = col->col_index;
      gc.codebook = col->codebook;
      gc.filter = col->filter;
      gc.most_common_value = col->most_common_value;
      gc.uses_shared_codebook = shared_codebook;
      gc.max_codelength = col->codebook ? col->codebook->max_codelength : 0;
      group.columns.push_back(std::move(gc));
    }
    return group;
  }

  // All columns of the group share the same key partition (identical
  // active_keys + seed), so partition the keys once and reuse that assignment
  // for every column; only the per-column values differ. We hold one shared
  // key_buckets plus each column's value_buckets — never M copies of the large
  // key signatures.
  const uint32_t M = static_cast<uint32_t>(group_cols.size());

  // Hash the keys ONCE (the dominant build cost) and reuse that partition for
  // every column, instead of re-hashing the same keys M times. fillArena then
  // scatters + solves one column at a time, so we never materialize all M
  // columns' bucketed values at once.
  const bool timing = std::getenv("CARAMEL_TIMING") != nullptr;
  Timer part_timer;
  KeyPartition part =
      partitionKeys(active_keys, static_cast<uint32_t>(num_buckets));
  uint64_t chosen_seed = part.seed;
  double partition_s = part_timer.seconds();

  // Hand each column's values to fillArena, which consumes them one column at a
  // time. Columns that resolved to stored values (filtered columns) are moved
  // in; no-filter columns are left empty and re-extracted on demand from `data`
  // by fillArena, so all M columns' values are never resident at once.
  std::vector<std::vector<T>> source_values_per_col(M);
  std::vector<uint32_t> col_indices(M);
  for (uint32_t c = 0; c < M; c++) {
    source_values_per_col[c] = std::move(group_cols[c]->values);
    col_indices[c] = group_cols[c]->col_index;
  }

  std::vector<std::shared_ptr<CsfCodebook<T>>> codebooks;
  codebooks.reserve(M);
  for (const auto *col : group_cols) {
    codebooks.push_back(col->codebook);
  }

  if (timing) {
    uint64_t kb_bytes = 0;
    for (const auto &bkt : part.key_buckets)
      kb_bytes += static_cast<uint64_t>(bkt.capacity()) * sizeof(__uint128_t);
    uint64_t cb_entries = 0;
    for (const auto *col : group_cols)
      if (col->codebook)
        cb_entries += col->codebook->codedict.size();
    std::cerr << "[timing] buildGroup M=" << M << " keys=" << active_keys.size()
              << " partition=" << partition_s << "s key_buckets=" << kb_bytes / 1e9
              << "GB codedict_entries=" << cb_entries << "\n";
  }

  group.hash_store_seed = static_cast<uint32_t>(chosen_seed);
  // Per-column codebooks are unique to their column, so fillArena may free each
  // codedict as it finishes. A shared codebook is reused across columns/groups,
  // so it must outlive the whole build (freed once at the end by the caller).
  fillArena<T>(group.arena, part, source_values_per_col, codebooks, col_indices,
               data, num_columns, /*codebooks_disposable=*/!shared_codebook,
               DELTA);

  group.columns.reserve(M);
  for (const auto *col : group_cols) {
    GroupColumnT gc;
    gc.output_index = col->col_index;
    gc.codebook = col->codebook;
    gc.filter = col->filter;
    gc.most_common_value = col->most_common_value;
    gc.uses_shared_codebook = shared_codebook;
    gc.max_codelength = col->codebook->max_codelength;
    group.columns.push_back(std::move(gc));
  }
  return group;
}

// Groups resolved columns by their shared active-key set. Columns share a
// group iff (a) they share the same filter pointer (or all have nullptr), and
// (b) they have identical active_keys (invariant guaranteed by construction:
// same filter ⇒ same filtered key set; no filter ⇒ full key set).
template <typename T>
std::vector<std::vector<ResolvedColumn<T> *>>
groupColumnsByActiveKeys(std::vector<ResolvedColumn<T>> &cols) {
  std::unordered_map<const void *, std::vector<ResolvedColumn<T> *>> by_filter;
  std::vector<const void *> order;  // preserve first-seen order
  for (auto &col : cols) {
    const void *key = col.filter.get();  // nullptr allowed
    if (by_filter.find(key) == by_filter.end()) {
      order.push_back(key);
    }
    by_filter[key].push_back(&col);
  }
  std::vector<std::vector<ResolvedColumn<T> *>> groups;
  groups.reserve(order.size());
  for (const void *key : order) {
    groups.push_back(std::move(by_filter[key]));
  }
  return groups;
}

// Splits a filter-group into sub-groups of columns that want a similar bucket
// count. Every column in a group shares one num_buckets (the group max) so a
// query computes one bucket id and streams the bucket's columns contiguously;
// without this split one high-entropy column would force the rest to
// over-partition, inflating build time. Columns are binned so the max/min
// target bucket count within a sub-group stays within kBucketRatio.
template <typename T>
std::vector<std::vector<ResolvedColumn<T> *>>
subGroupByBucketCount(const std::vector<ResolvedColumn<T> *> &group_cols) {
  constexpr uint64_t kBucketRatio = 2;
  std::vector<std::pair<uint64_t, ResolvedColumn<T> *>> with_nb;
  with_nb.reserve(group_cols.size());
  for (auto *col : group_cols) {
    with_nb.emplace_back(col->num_buckets, col);
  }
  std::sort(with_nb.begin(), with_nb.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });

  std::vector<std::vector<ResolvedColumn<T> *>> sub_groups;
  uint64_t sub_min = 0;
  for (auto &[nb, col] : with_nb) {
    if (sub_groups.empty() || nb > sub_min * kBucketRatio) {
      sub_groups.push_back({});
      sub_min = nb;
    }
    sub_groups.back().push_back(col);
  }
  return sub_groups;
}

template <typename T>
MultisetCsfPtr<T>
constructMultisetCsf(const std::vector<std::string> &keys,
                     std::vector<std::vector<T>> values,
                     const MultisetConfig &config) {
  size_t num_columns = values.size();

  if (config.permutation_config && num_columns > 1) {
    if (std::dynamic_pointer_cast<EntropyPermutationConfig>(
            config.permutation_config)) {
      applyPermutation(values, entropyPermutation<T>);
    } else if (auto cfg =
                   std::dynamic_pointer_cast<GlobalSortPermutationConfig>(
                       config.permutation_config)) {
      int iters = cfg->refinement_iterations;
      applyPermutation(values, [iters](T *M, int nr, int nc) {
        globalSortPermutation<T>(M, nr, nc, iters);
      });
    }
  }

  // Shared codebook: pool all columns' values, compute one Huffman tree
  std::shared_ptr<CsfCodebook<T>> shared_cb;
  if (config.shared_codebook) {
    std::vector<T> pooled;
    for (size_t i = 0; i < num_columns; i++) {
      pooled.insert(pooled.end(), values[i].begin(), values[i].end());
    }
    shared_cb = std::make_shared<CsfCodebook<T>>(canonicalHuffman<T>(pooled));
  }

  std::vector<ColumnFilterInfo<T>> group_filters;
  if (config.shared_filter && config.filter_config) {
    group_filters = buildGroupSharedFilters<T>(keys, values, config.filter_config, config.verbose);
  }

  std::vector<ResolvedColumn<T>> resolved(num_columns);
  for (size_t i = 0; i < num_columns; i++) {
    auto *col_filter = group_filters.empty() ? nullptr : &group_filters[i];
    auto col_inputs = resolveColumnInputs<T>(
        keys, values[i], col_filter, config.filter_config, config.verbose);

    bool using_filter = (col_inputs.filter != nullptr);
    resolved[i].col_index = static_cast<uint32_t>(i);
    resolved[i].filter = col_inputs.filter;
    resolved[i].most_common_value = col_inputs.most_common_value;
    // No-filter columns share the full key set, so leave ResolvedColumn::keys
    // empty and let buildGroup fall back to the shared `keys` reference
    // (zero-copy). Only filtered columns own a distinct key subset.
    if (using_filter) {
      resolved[i].keys = std::move(col_inputs.keys);
    }
    resolved[i].values =
        using_filter ? std::move(col_inputs.values) : std::move(values[i]);
    resolved[i].codebook =
        columnCodebook<T>(resolved[i].values, config.shared_codebook, shared_cb);
    resolved[i].num_buckets =
        resolved[i].codebook
            ? targetBucketCount(resolved[i].values,
                                resolved[i].codebook->codedict)
            : 0;
  }

  std::vector<typename MultisetCsf<T>::Group> groups;
  for (auto &filter_group : groupColumnsByActiveKeys<T>(resolved)) {
    for (auto &group_cols : subGroupByBucketCount<T>(filter_group)) {
      const auto &active_keys =
          group_cols.front()->filter ? group_cols.front()->keys : keys;
      groups.push_back(
          buildGroup<T>(active_keys, group_cols, config.shared_codebook));
    }
  }

  // Shared codebook reused across groups; free its construction-only codedict
  // now that every group is solved (queries never touch it).
  if (config.shared_codebook && shared_cb) {
    shared_cb->codedict = CodeDict<T>();
  }

  return std::make_shared<MultisetCsf<T>>(
      std::move(groups), static_cast<uint32_t>(num_columns), shared_cb);
}

template <typename T>
struct MultisetConstructionResult {
  MultisetCsfPtr<T> csf;
  double permutation_seconds = 0.0;
  double build_seconds = 0.0;
};

// Constructs a MultisetCsf from a row-major flat buffer (T* data, num_rows x
// num_cols). Avoids the memory doubling of the vector<vector<T>> overload by
// extracting columns on-the-fly. Returns timing stats for permutation and build.
template <typename T>
MultisetConstructionResult<T>
constructMultisetCsfRowMajor(const std::vector<std::string> &keys,
                             T *data, int num_rows, int num_cols,
                             const MultisetConfig &config) {
  MultisetConstructionResult<T> result;
  Timer timer;

  if (config.permutation_config && num_cols > 1) {
    if (std::dynamic_pointer_cast<EntropyPermutationConfig>(
            config.permutation_config)) {
      entropyPermutation<T>(data, num_rows, num_cols);
    } else if (auto cfg =
                   std::dynamic_pointer_cast<GlobalSortPermutationConfig>(
                       config.permutation_config)) {
      globalSortPermutation<T>(data, num_rows, num_cols,
                               cfg->refinement_iterations);
    }
  }
  result.permutation_seconds = timer.seconds();

  size_t num_columns = static_cast<size_t>(num_cols);
  size_t n = static_cast<size_t>(num_rows);

  // Shared codebook: stream a frequency histogram instead of copying the
  // whole flat buffer.
  //
  // Parallelized via hash partitioning: each key is owned by exactly one
  // thread (keyed by a mixing hash), so peak memory stays ~1x the final
  // histogram size instead of T x. Every thread scans the entire buffer and
  // cheaply skips keys it doesn't own.
  std::shared_ptr<CsfCodebook<T>> shared_cb;
  if (config.shared_codebook) {
    size_t total = n * num_columns;
    const int freq_threads = std::min(8, omp_get_max_threads());
    std::vector<std::unordered_map<T, uint64_t>> partial_freqs(freq_threads);

    if (config.verbose) {
      std::cout << "  Building pooled frequency histogram over " << total
                << " cells (" << freq_threads << " threads)..." << std::endl;
    }
    Timer freq_timer;

#pragma omp parallel num_threads(freq_threads) default(none)                   \
    shared(partial_freqs, data, total, freq_threads)
    {
      int tid = omp_get_thread_num();
      auto &local = partial_freqs[tid];
      const uint32_t t = static_cast<uint32_t>(freq_threads);
      const uint32_t owner = static_cast<uint32_t>(tid);
      for (size_t i = 0; i < total; i++) {
        T key = data[i];
        uint32_t h = static_cast<uint32_t>(key);
        h ^= h >> 16;
        h *= 0x85ebca6b;
        h ^= h >> 13;
        if (h % t == owner) {
          ++local[key];
        }
      }
    }

    // Hash partitioning guarantees disjoint key sets, so the merge is a
    // plain union — no collisions to resolve.
    std::unordered_map<T, uint64_t> freqs;
    size_t total_unique = 0;
    for (const auto &p : partial_freqs) total_unique += p.size();
    freqs.reserve(total_unique);
    for (auto &p : partial_freqs) {
      freqs.insert(p.begin(), p.end());
      p = {};
    }

    if (config.verbose) {
      std::cout << "  Histogram done in " << freq_timer.seconds() << "s ("
                << total_unique << " unique values)" << std::endl;
    }

    shared_cb = std::make_shared<CsfCodebook<T>>(
        canonicalHuffmanFromFrequencies<T>(freqs));
  }

  std::vector<ColumnFilterInfo<T>> group_filters;
  if (config.shared_filter && config.filter_config) {
    std::vector<std::vector<T>> col_values(num_columns);
    for (size_t c = 0; c < num_columns; c++) {
      col_values[c].resize(n);
      for (size_t r = 0; r < n; r++) {
        col_values[c][r] = data[r * num_columns + c];
      }
    }
    group_filters = buildGroupSharedFilters<T>(keys, col_values,
                                               config.filter_config,
                                               config.verbose);
  }

  std::vector<ResolvedColumn<T>> resolved(num_columns);
  std::vector<T> column_values(n);

  for (size_t i = 0; i < num_columns; i++) {
    for (size_t r = 0; r < n; r++) {
      column_values[r] = data[r * num_columns + i];
    }

    auto *col_filter = group_filters.empty() ? nullptr : &group_filters[i];
    auto col_inputs = resolveColumnInputs<T>(
        keys, column_values, col_filter, config.filter_config, config.verbose);

    bool using_filter = (col_inputs.filter != nullptr);
    resolved[i].col_index = static_cast<uint32_t>(i);
    resolved[i].filter = col_inputs.filter;
    resolved[i].most_common_value = col_inputs.most_common_value;
    // No-filter columns share the full key set, so leave keys empty and let
    // the call site fall back to the shared `keys` reference (zero-copy).
    if (using_filter) {
      resolved[i].keys = std::move(col_inputs.keys);
    }
    // Codebook + bucket count are computed now, from the active values. For
    // no-filter columns we then drop the values (left empty) -- buildGroup
    // re-extracts them column-by-column from `data`, so we never hold all M
    // columns' values at once. Filtered columns keep their (smaller) values.
    const std::vector<T> &active_values =
        using_filter ? col_inputs.values : column_values;
    resolved[i].codebook =
        columnCodebook<T>(active_values, config.shared_codebook, shared_cb);
    resolved[i].num_buckets =
        resolved[i].codebook
            ? targetBucketCount(active_values, resolved[i].codebook->codedict)
            : 0;
    if (using_filter) {
      resolved[i].values = std::move(col_inputs.values);
    }
  }

  std::vector<typename MultisetCsf<T>::Group> groups;
  for (auto &filter_group : groupColumnsByActiveKeys<T>(resolved)) {
    for (auto &group_cols : subGroupByBucketCount<T>(filter_group)) {
      const auto &active_keys =
          group_cols.front()->filter ? group_cols.front()->keys : keys;
      groups.push_back(buildGroup<T>(active_keys, group_cols,
                                     config.shared_codebook, data, num_columns));
    }
  }

  // A shared codebook is reused across all groups, so it's freed here (once all
  // groups are solved) rather than progressively inside fillArena. codedict is
  // construction-only; queries never touch it.
  if (config.shared_codebook && shared_cb) {
    shared_cb->codedict = CodeDict<T>();
  }

  result.build_seconds = timer.seconds();
  result.csf = std::make_shared<MultisetCsf<T>>(
      std::move(groups), static_cast<uint32_t>(num_columns), shared_cb);
  return result;
}

} // namespace caramel
