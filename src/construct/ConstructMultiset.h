#pragma once

#include "Construct.h"
#include "MultisetCsf.h"
#include "filter/FilterFactory.h"

namespace caramel {

template <typename T>
MultisetCsfPtr<T>
constructMultisetCsf(const std::vector<std::string> &keys,
                     const std::vector<std::vector<T>> &values,
                     PreFilterConfigPtr filter_config = nullptr,
                     bool verbose = true) {
  // TODO(david) shared codedict option, store it once in multisetcsf
  // TODO(david) don't recompute the bucketedhashstore every time if not needed
  size_t num_columns = values.size();

  std::vector<CsfPtr<T>> csfs(num_columns);

  // Adding a pragma here was slightly faster in some cases and slower in
  // others. It seemed to be dependent on the number of columns and the
  // size/distribution of the dataset. However, even in the cases it was faster
  // it wasn't by much, < 10%, in which case its probably not super worthwhile
  // to figure out the optimal condition for adding parallelism.
  for (size_t i = 0; i < num_columns; i++) {
    std::vector<std::string> filtered_keys_storage;
    std::vector<T> filtered_values_storage;

    PreFilterPtr<T> filter = nullptr;
    if (filter_config) {
      filtered_keys_storage = keys;
      filtered_values_storage = values[i];
      filter = FilterFactory::makeFilter<T>(filter_config);
      filter->apply(filtered_keys_storage, filtered_values_storage, DELTA,
                    verbose);
    }

    const auto &active_keys = filter_config ? filtered_keys_storage : keys;
    const auto &active_values =
        filter_config ? filtered_values_storage : values[i];

    HuffmanOutput<T> huffman = canonicalHuffman<T>(active_values);

    uint64_t num_buckets = targetBucketCount(active_values, huffman.codedict);

    BucketedHashStore<T> hash_store =
        partitionToBuckets<T>(active_keys, active_values, num_buckets);

    std::exception_ptr exception = nullptr;
    std::vector<SubsystemSolutionSeedPair> solutions_and_seeds(
        hash_store.num_buckets);

#pragma omp parallel for default(none)                                         \
    shared(hash_store, huffman, solutions_and_seeds, exception, DELTA)
    for (uint32_t i = 0; i < hash_store.num_buckets; i++) {
      if (exception) {
        continue;
      }
      try {
        solutions_and_seeds[i] = constructAndSolveSubsystem<T>(
            hash_store.key_buckets[i], hash_store.value_buckets[i],
            huffman.codedict, huffman.max_codelength, DELTA);
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

    csfs[i] = Csf<T>::make(solutions_and_seeds, huffman.code_length_counts,
                           huffman.ordered_symbols, hash_store.seed, filter);
  }

  return std::make_shared<MultisetCsf<T>>(csfs);
}

} // namespace caramel