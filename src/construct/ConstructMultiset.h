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
    std::vector<std::string> filtered_keys = keys;
    std::vector<T> filtered_values = values[i];

    PreFilterPtr<T> filter = nullptr;
    if (filter_config) {
      filter = FilterFactory::makeFilter<T>(filter_config);
      filter->apply(filtered_keys, filtered_values, DELTA, verbose);
    }

    HuffmanOutput<T> huffman = cannonicalHuffman<T>(filtered_values);

    double avg_bits_per_key = 0;
    for (const auto &v : filtered_values) {
      avg_bits_per_key += huffman.codedict.at(v).numBits();
    }
    avg_bits_per_key /= filtered_values.size();

    uint64_t bucket_size =
        static_cast<uint64_t>(3500.0 / avg_bits_per_key);
    if (bucket_size > 1000) {
      bucket_size = 1000;
    } else if (bucket_size < 100) {
      bucket_size = 100;
    }

    BucketedHashStore<T> hash_store =
        partitionToBuckets<T>(filtered_keys, filtered_values, bucket_size);

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