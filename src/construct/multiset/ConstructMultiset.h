#pragma once

#include "src/construct/Construct.h"
#include "src/construct/multiset/EntropyPermutation.h"
#include "src/construct/multiset/MultisetCsf.h"
#include "src/construct/multiset/MultisetConfig.h"
#include "src/construct/filter/FilterFactory.h"

namespace caramel {

template <typename T>
MultisetCsfPtr<T>
constructMultisetCsf(const std::vector<std::string> &keys,
                     std::vector<std::vector<T>> values,
                     const MultisetConfig &config) {
  size_t num_columns = values.size();

  if (config.permutation == PermutationStrategy::Entropy && num_columns > 1) {
    size_t num_rows = values[0].size();
    // Column-major → row-major flat buffer
    std::vector<T> buf(num_rows * num_columns);
    for (size_t c = 0; c < num_columns; c++) {
      for (size_t r = 0; r < num_rows; r++) {
        buf[r * num_columns + c] = values[c][r];
      }
    }
    entropyPermutation<T>(buf.data(), num_rows, num_columns);
    // Row-major flat buffer → column-major
    for (size_t c = 0; c < num_columns; c++) {
      for (size_t r = 0; r < num_rows; r++) {
        values[c][r] = buf[r * num_columns + c];
      }
    }
  }

  using ColumnState = typename MultisetCsf<T>::ColumnState;
  std::vector<ColumnState> columns(num_columns);

  // Shared codebook: pool all columns' values, compute one Huffman
  std::shared_ptr<CsfCodebook<T>> shared_cb;
  if (config.shared_codebook) {
    std::vector<T> pooled;
    for (size_t i = 0; i < num_columns; i++) {
      pooled.insert(pooled.end(), values[i].begin(), values[i].end());
    }
    shared_cb = std::make_shared<CsfCodebook<T>>(canonicalHuffman<T>(pooled));
  }

  // Shared filter: find global MCV, build one filter for all columns
  PreFilterPtr<T> shared_filter;
  T global_mcv{};
  std::vector<bool> is_minority_key;
  if (config.shared_filter && config.filter_config) {
    // Find global most common value across entire matrix
    std::unordered_map<T, size_t> frequencies;
    for (size_t i = 0; i < num_columns; i++) {
      for (const auto &v : values[i]) {
        frequencies[v]++;
      }
    }
    size_t highest_freq = 0;
    for (const auto &[val, freq] : frequencies) {
      if (freq >= highest_freq) {
        highest_freq = freq;
        global_mcv = val;
      }
    }

    // A key is minority if ANY column's value differs from global MCV
    size_t num_keys = keys.size();
    is_minority_key.resize(num_keys, false);
    for (size_t k = 0; k < num_keys; k++) {
      for (size_t i = 0; i < num_columns; i++) {
        if (values[i][k] != global_mcv) {
          is_minority_key[k] = true;
          break;
        }
      }
    }

    // Build filter using synthetic values: minority keys get a sentinel,
    // majority keys get global_mcv. This reuses PreFilter::apply() which
    // finds MCV and adds non-MCV keys to the filter.
    T sentinel = (frequencies.size() > 1)
                     ? frequencies.begin()->first
                     : global_mcv;
    // Make sure sentinel != global_mcv
    for (const auto &[val, _] : frequencies) {
      if (val != global_mcv) {
        sentinel = val;
        break;
      }
    }

    std::vector<T> synthetic_values(num_keys);
    for (size_t k = 0; k < num_keys; k++) {
      synthetic_values[k] = is_minority_key[k] ? sentinel : global_mcv;
    }

    shared_filter = FilterFactory::makeFilter<T>(config.filter_config);
    std::vector<std::string> filtered_keys_out;
    std::vector<T> filtered_values_out;
    shared_filter->apply(keys, synthetic_values, filtered_keys_out,
                         filtered_values_out, DELTA, config.verbose);
  }

  for (size_t i = 0; i < num_columns; i++) {
    std::vector<std::string> filtered_keys_storage;
    std::vector<T> filtered_values_storage;

    PreFilterPtr<T> filter = nullptr;
    if (config.shared_filter && shared_filter) {
      filter = shared_filter;
      // For shared filter: CSF must encode ALL minority keys
      filtered_keys_storage.reserve(keys.size());
      filtered_values_storage.reserve(keys.size());
      for (size_t k = 0; k < keys.size(); k++) {
        if (is_minority_key[k]) {
          filtered_keys_storage.push_back(keys[k]);
          filtered_values_storage.push_back(values[i][k]);
        }
      }
    } else if (config.filter_config && !config.shared_filter) {
      filter = FilterFactory::makeFilter<T>(config.filter_config);
      filter->apply(keys, values[i], filtered_keys_storage,
                    filtered_values_storage, DELTA, config.verbose);
    }

    bool using_filter = (filter != nullptr);
    const auto &active_keys = using_filter ? filtered_keys_storage : keys;
    const auto &active_values =
        using_filter ? filtered_values_storage : values[i];

    std::shared_ptr<CsfCodebook<T>> col_codebook;
    if (config.shared_codebook) {
      col_codebook = shared_cb;
    } else {
      col_codebook = std::make_shared<CsfCodebook<T>>(
          canonicalHuffman<T>(active_values));
    }

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
    columns[i].filter = filter;
    if (config.shared_filter && shared_filter) {
      columns[i].most_common_value = global_mcv;
    } else {
      columns[i].most_common_value =
          filter ? filter->getMostCommonValue() : std::nullopt;
    }
    columns[i].buildQueryCache();
  }

  return std::make_shared<MultisetCsf<T>>(std::move(columns));
}

} // namespace caramel
