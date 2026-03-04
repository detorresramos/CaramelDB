#pragma once

#include "src/construct/Construct.h"
#include "src/construct/multiset/EntropyPermutation.h"
#include "src/construct/multiset/MultisetCsf.h"
#include "src/construct/multiset/MultisetConfig.h"
#include "src/construct/filter/FilterFactory.h"

namespace caramel {

template <typename T>
void applyEntropyPermutation(std::vector<std::vector<T>> &values) {
  size_t num_columns = values.size();
  size_t num_rows = values[0].size();

  std::vector<T> buf(num_rows * num_columns);
  for (size_t c = 0; c < num_columns; c++) {
    for (size_t r = 0; r < num_rows; r++) {
      buf[r * num_columns + c] = values[c][r];
    }
  }
  entropyPermutation<T>(buf.data(), num_rows, num_columns);
  for (size_t c = 0; c < num_columns; c++) {
    for (size_t r = 0; r < num_rows; r++) {
      values[c][r] = buf[r * num_columns + c];
    }
  }
}

template <typename T>
struct SharedFilterResult {
  PreFilterPtr<T> filter;
  T global_mcv{};
  std::vector<bool> is_minority_key;
};

// Builds a single filter shared across all columns. A key is "minority" (stored
// in the CSF) if ANY of its column values differ from the global most common
// value; majority keys are handled by the filter alone.
template <typename T>
SharedFilterResult<T>
buildSharedFilter(const std::vector<std::string> &keys,
                  const std::vector<std::vector<T>> &values,
                  const MultisetConfig &config) {
  size_t num_columns = values.size();
  size_t num_keys = keys.size();

  std::unordered_map<T, size_t> frequencies;
  for (size_t i = 0; i < num_columns; i++) {
    for (const auto &v : values[i]) {
      frequencies[v]++;
    }
  }

  T global_mcv{};
  size_t highest_freq = 0;
  for (const auto &[val, freq] : frequencies) {
    if (freq >= highest_freq) {
      highest_freq = freq;
      global_mcv = val;
    }
  }

  std::vector<bool> is_minority_key(num_keys, false);
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
  T sentinel = global_mcv;
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

  auto filter = FilterFactory::makeFilter<T>(config.filter_config);
  std::vector<std::string> filtered_keys_out;
  std::vector<T> filtered_values_out;
  filter->apply(keys, synthetic_values, filtered_keys_out,
                filtered_values_out, DELTA, config.verbose);

  return {filter, global_mcv, std::move(is_minority_key)};
}

template <typename T>
struct ColumnInputs {
  PreFilterPtr<T> filter;
  std::vector<std::string> keys;
  std::vector<T> values;
  std::optional<T> most_common_value;
};

// Resolves which keys/values a column's CSF needs to encode, depending on
// whether we're using a shared filter, per-column filter, or no filter.
template <typename T>
ColumnInputs<T>
resolveColumnInputs(const std::vector<std::string> &all_keys,
                    const std::vector<T> &column_values,
                    const SharedFilterResult<T> *shared,
                    const MultisetConfig &config) {
  if (shared) {
    std::vector<std::string> keys;
    std::vector<T> values;
    keys.reserve(all_keys.size());
    values.reserve(all_keys.size());
    for (size_t k = 0; k < all_keys.size(); k++) {
      if (shared->is_minority_key[k]) {
        keys.push_back(all_keys[k]);
        values.push_back(column_values[k]);
      }
    }
    return {shared->filter, std::move(keys), std::move(values),
            shared->global_mcv};
  }

  if (config.filter_config) {
    auto filter = FilterFactory::makeFilter<T>(config.filter_config);
    std::vector<std::string> keys;
    std::vector<T> values;
    filter->apply(all_keys, column_values, keys, values, DELTA, config.verbose);
    std::optional<T> mcv = filter->getMostCommonValue();
    return {filter, std::move(keys), std::move(values), mcv};
  }

  return {nullptr, {}, {}, std::nullopt};
}

template <typename T>
MultisetCsfPtr<T>
constructMultisetCsf(const std::vector<std::string> &keys,
                     std::vector<std::vector<T>> values,
                     const MultisetConfig &config) {
  size_t num_columns = values.size();

  if (config.permutation == PermutationStrategy::Entropy && num_columns > 1) {
    applyEntropyPermutation(values);
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

  // Shared filter
  std::unique_ptr<SharedFilterResult<T>> shared;
  if (config.shared_filter && config.filter_config) {
    shared = std::make_unique<SharedFilterResult<T>>(
        buildSharedFilter<T>(keys, values, config));
  }

  using ColumnState = typename MultisetCsf<T>::ColumnState;
  std::vector<ColumnState> columns(num_columns);

  for (size_t i = 0; i < num_columns; i++) {
    auto col_inputs = resolveColumnInputs<T>(
        keys, values[i], shared.get(), config);

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
    columns[i].filter = col_inputs.filter;
    columns[i].most_common_value = col_inputs.most_common_value;
    columns[i].buildQueryCache();
  }

  return std::make_shared<MultisetCsf<T>>(std::move(columns));
}

} // namespace caramel
