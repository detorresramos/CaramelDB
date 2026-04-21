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

  using ColumnState = typename MultisetCsf<T>::ColumnState;
  std::vector<ColumnState> columns(num_columns);

  for (size_t i = 0; i < num_columns; i++) {
    auto *col_filter = group_filters.empty() ? nullptr : &group_filters[i];
    auto col_inputs = resolveColumnInputs<T>(
        keys, values[i], col_filter, config.filter_config, config.verbose);

    bool using_filter = (col_inputs.filter != nullptr);
    const auto &active_keys = using_filter ? col_inputs.keys : keys;
    const auto &active_values = using_filter ? col_inputs.values : values[i];

    auto col_codebook = config.shared_codebook
        ? shared_cb
        : std::make_shared<CsfCodebook<T>>(canonicalHuffman<T>(active_values));

    const CodeDict<T> &codedict = col_codebook->codedict;
    uint32_t max_cl = col_codebook->max_codelength;

    uint64_t num_buckets = targetBucketCount(active_values, codedict);

    BucketedHashStore<T> hash_store =
        partitionToBuckets<T>(active_keys, active_values, num_buckets);

    std::exception_ptr exception = nullptr;
    std::vector<SubsystemSolutionSeedPair> solutions_and_seeds(
        hash_store.num_buckets);

#pragma omp parallel for default(none)                                         \
    shared(hash_store, solutions_and_seeds, exception, DELTA,                  \
           codedict, max_cl)
    for (uint32_t j = 0; j < hash_store.num_buckets; j++) {
      if (exception) {
        continue;
      }
      try {
        solutions_and_seeds[j] = constructAndSolveSubsystem<T>(
            hash_store.key_buckets[j], hash_store.value_buckets[j],
            codedict, max_cl, DELTA);
      } catch (std::exception &e) {
#pragma omp critical
        {
          exception = std::current_exception();
        }
      }
    }

    if (exception) {
      std::rethrow_exception(exception);
    }

    columns[i].solutions_and_seeds = std::move(solutions_and_seeds);
    columns[i].hash_store_seed = hash_store.seed;
    columns[i].codebook = col_codebook;
    columns[i].uses_shared_codebook = config.shared_codebook;
    columns[i].filter = col_inputs.filter;
    columns[i].most_common_value = col_inputs.most_common_value;
    columns[i].buildQueryCache();
  }

  return std::make_shared<MultisetCsf<T>>(std::move(columns), shared_cb);
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

  using ColumnState = typename MultisetCsf<T>::ColumnState;
  std::vector<ColumnState> columns(num_columns);
  std::vector<T> column_values(n);

  for (size_t i = 0; i < num_columns; i++) {
    for (size_t r = 0; r < n; r++) {
      column_values[r] = data[r * num_columns + i];
    }

    auto *col_filter = group_filters.empty() ? nullptr : &group_filters[i];
    auto col_inputs = resolveColumnInputs<T>(
        keys, column_values, col_filter, config.filter_config, config.verbose);

    bool using_filter = (col_inputs.filter != nullptr);
    const auto &active_keys = using_filter ? col_inputs.keys : keys;
    const auto &active_values = using_filter ? col_inputs.values : column_values;

    auto col_codebook = config.shared_codebook
        ? shared_cb
        : std::make_shared<CsfCodebook<T>>(canonicalHuffman<T>(active_values));

    const CodeDict<T> &codedict = col_codebook->codedict;
    uint32_t max_cl = col_codebook->max_codelength;

    uint64_t num_buckets = targetBucketCount(active_values, codedict);

    BucketedHashStore<T> hash_store =
        partitionToBuckets<T>(active_keys, active_values, num_buckets);

    std::exception_ptr exception = nullptr;
    std::vector<SubsystemSolutionSeedPair> solutions_and_seeds(
        hash_store.num_buckets);

#pragma omp parallel for default(none)                                         \
    shared(hash_store, solutions_and_seeds, exception, DELTA,                  \
           codedict, max_cl)
    for (uint32_t j = 0; j < hash_store.num_buckets; j++) {
      if (exception) {
        continue;
      }
      try {
        solutions_and_seeds[j] = constructAndSolveSubsystem<T>(
            hash_store.key_buckets[j], hash_store.value_buckets[j],
            codedict, max_cl, DELTA);
      } catch (std::exception &e) {
#pragma omp critical
        {
          exception = std::current_exception();
        }
      }
    }

    if (exception) {
      std::rethrow_exception(exception);
    }

    columns[i].solutions_and_seeds = std::move(solutions_and_seeds);
    columns[i].hash_store_seed = hash_store.seed;
    columns[i].codebook = col_codebook;
    columns[i].filter = col_inputs.filter;
    columns[i].most_common_value = col_inputs.most_common_value;
    columns[i].buildQueryCache();
  }

  result.build_seconds = timer.seconds();
  result.csf = std::make_shared<MultisetCsf<T>>(std::move(columns));
  return result;
}

} // namespace caramel
