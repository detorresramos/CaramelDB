#pragma once

#include "src/construct/Construct.h"
#include "src/construct/multiset/permute/EntropyPermutation.h"
#include "src/construct/multiset/permute/GlobalSortPermutation.h"
#include "src/construct/multiset/MultisetCsf.h"
#include "src/construct/multiset/MultisetConfig.h"
#include "src/construct/filter/FilterFactory.h"
#include "src/utils/Timer.h"
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
  std::vector<T> values;          // active values
  std::shared_ptr<CsfCodebook<T>> codebook;
};

// Packs solved per-(bucket, col) BitArrays into a flat interleaved arena.
// Each (bucket, col) range is word-aligned (start bit % 64 == 0) so the
// query path can reuse queryCsfCore's bit-slicing unchanged. Intra-bucket
// padding is at most 63 bits per column — negligible vs per-column
// BitArrayPtr allocations.
template <typename T>
void packArena(
    typename MultisetCsf<T>::BucketArena &arena,
    const std::vector<std::vector<SubsystemSolutionSeedPair>> &per_col_solutions,
    uint32_t num_buckets) {
  const uint32_t M = static_cast<uint32_t>(per_col_solutions.size());
  arena.num_cols = M;
  arena.num_buckets = num_buckets;
  arena.bucket_offsets.assign(num_buckets + 1, 0);
  arena.per_col_bits.assign(static_cast<size_t>(num_buckets) * M, 0);
  arena.per_col_seeds.assign(static_cast<size_t>(num_buckets) * M, 0);

  // First pass: compute padded bit length for each (bucket, col) and the
  // bucket block offsets. Each sub-range is padded up to a 64-bit word
  // boundary so word-indexed reads in queryCsfCore work directly.
  uint64_t cursor_bits = 0;
  std::vector<uint32_t> padded_bits(static_cast<size_t>(num_buckets) * M, 0);
  for (uint32_t b = 0; b < num_buckets; b++) {
    arena.bucket_offsets[b] = cursor_bits;
    for (uint32_t c = 0; c < M; c++) {
      const auto &[solution, seed] = per_col_solutions[c][b];
      uint32_t bits = solution->numBits();
      arena.per_col_bits[b * M + c] = bits;
      arena.per_col_seeds[b * M + c] = seed;
      uint32_t padded = (bits + 63u) & ~63u;
      padded_bits[b * M + c] = padded;
      cursor_bits += padded;
    }
  }
  arena.bucket_offsets[num_buckets] = cursor_bits;

  // Second pass: allocate and copy bits. Each sub-range starts word-aligned,
  // so we can memcpy word-by-word from the BitArray's backing array.
  uint64_t total_words = cursor_bits / 64;
  arena.solution_bits.assign(total_words, 0);

  for (uint32_t b = 0; b < num_buckets; b++) {
    uint64_t bit_off = arena.bucket_offsets[b];
    for (uint32_t c = 0; c < M; c++) {
      const auto &solution = per_col_solutions[c][b].first;
      uint32_t bits = solution->numBits();
      uint32_t padded = padded_bits[b * M + c];
      uint64_t word_off = bit_off / 64;
      const uint64_t *src = solution->backingArrayPtr();
      uint32_t src_words = (bits + 63u) / 64u;
      std::copy(src, src + src_words, arena.solution_bits.data() + word_off);
      bit_off += padded;
    }
  }
}

// Runs partitionToBuckets once for a group of columns that share active keys,
// then solves each column's subsystems against that single hash store and
// packs all solutions into a flat interleaved arena.
template <typename T>
typename MultisetCsf<T>::Group
buildGroup(const std::vector<std::string> &active_keys,
           const std::vector<ResolvedColumn<T> *> &group_cols,
           bool shared_codebook) {
  using GroupT = typename MultisetCsf<T>::Group;
  using GroupColumnT = typename MultisetCsf<T>::GroupColumn;
  GroupT group;

  // Group-level num_buckets = max across member columns. This guarantees
  // every column fits in the shared bucket layout.
  uint64_t num_buckets = 0;
  for (const auto *col : group_cols) {
    uint64_t nb = targetBucketCount(col->values, col->codebook->codedict);
    num_buckets = std::max(num_buckets, nb);
  }

  if (num_buckets == 0 || active_keys.empty()) {
    // Degenerate group: all keys were filtered out for every column. Keep
    // zero-bucket arena; query returns the most-common value per column.
    group.num_buckets = 0;
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

  // Per-column hash stores share a seed and bucket layout. We build one
  // BucketedHashStore per column (each column may have different values
  // for the same key, so value_buckets differ), but all share the same
  // key partitioning because active_keys is identical across the group.
  std::vector<BucketedHashStore<T>> per_col_stores(group_cols.size());
  uint64_t chosen_seed = 0;
  {
    // Try partitioning with the first column's values to pick a seed that
    // passes the collision check. The collision check only depends on the
    // 128-bit key hashes, so any column's values will accept the same seed.
    per_col_stores[0] = partitionToBuckets<T>(active_keys, group_cols[0]->values,
                                              num_buckets);
    chosen_seed = per_col_stores[0].seed;
  }
  for (size_t i = 1; i < group_cols.size(); i++) {
    // Reuse the chosen seed; rebuild buckets with column i's values. This
    // guarantees all columns see the same key-to-bucket assignment.
    per_col_stores[i] = construct<T>(
        active_keys, group_cols[i]->values, num_buckets, chosen_seed,
        active_keys.size() / num_buckets + 1);
  }

  // Solve each column's subsystems (bucket-parallel within each column).
  const uint32_t M = static_cast<uint32_t>(group_cols.size());
  std::vector<std::vector<SubsystemSolutionSeedPair>> per_col_solutions(M);

  for (uint32_t c = 0; c < M; c++) {
    per_col_solutions[c].assign(num_buckets, {});
    const auto &store = per_col_stores[c];
    const CodeDict<T> &codedict = group_cols[c]->codebook->codedict;
    uint32_t max_cl = group_cols[c]->codebook->max_codelength;
    std::exception_ptr exception = nullptr;

#pragma omp parallel for default(none)                                         \
    shared(store, per_col_solutions, c, exception, codedict, max_cl, DELTA,    \
           num_buckets)
    for (uint32_t j = 0; j < num_buckets; j++) {
      if (exception) {
        continue;
      }
      try {
        per_col_solutions[c][j] = constructAndSolveSubsystem<T>(
            store.key_buckets[j], store.value_buckets[j], codedict, max_cl,
            DELTA);
      } catch (std::exception &) {
#pragma omp critical
        { exception = std::current_exception(); }
      }
    }

    if (exception) {
      std::rethrow_exception(exception);
    }
  }

  group.hash_store_seed = static_cast<uint32_t>(chosen_seed);
  group.num_buckets = static_cast<uint32_t>(num_buckets);
  packArena<T>(group.arena, per_col_solutions,
               static_cast<uint32_t>(num_buckets));

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
    resolved[i].keys = using_filter ? std::move(col_inputs.keys) : keys;
    resolved[i].values =
        using_filter ? std::move(col_inputs.values) : std::move(values[i]);
    resolved[i].codebook = config.shared_codebook
        ? shared_cb
        : std::make_shared<CsfCodebook<T>>(canonicalHuffman<T>(resolved[i].values));
  }

  auto col_groups = groupColumnsByActiveKeys<T>(resolved);
  std::vector<typename MultisetCsf<T>::Group> groups;
  groups.reserve(col_groups.size());
  for (auto &group_cols : col_groups) {
    const auto &active_keys = group_cols.front()->keys;
    groups.push_back(buildGroup<T>(active_keys, group_cols,
                                    config.shared_codebook));
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
    resolved[i].keys = using_filter ? std::move(col_inputs.keys) : keys;
    resolved[i].values =
        using_filter ? std::move(col_inputs.values) : column_values;
    resolved[i].codebook = config.shared_codebook
        ? shared_cb
        : std::make_shared<CsfCodebook<T>>(canonicalHuffman<T>(resolved[i].values));
  }

  auto col_groups = groupColumnsByActiveKeys<T>(resolved);
  std::vector<typename MultisetCsf<T>::Group> groups;
  groups.reserve(col_groups.size());
  for (auto &group_cols : col_groups) {
    const auto &active_keys = group_cols.front()->keys;
    groups.push_back(buildGroup<T>(active_keys, group_cols,
                                    config.shared_codebook));
  }

  result.build_seconds = timer.seconds();
  result.csf = std::make_shared<MultisetCsf<T>>(
      std::move(groups), static_cast<uint32_t>(num_columns), shared_cb);
  return result;
}

} // namespace caramel
