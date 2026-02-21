#pragma once

#include "BucketedHashStore.h"
#include "Codec.h"
#include "ConstructUtils.h"
#include "Csf.h"
#include <cmath>
#include <src/Modulo2System.h>
#include <src/construct/filter/FilterFactory.h>
#include <src/solve/Solve.h>
#include <src/utils/ProgressBar.h>
#include <src/utils/Timer.h>

namespace caramel {

/*
Constructs a binary system of linear equations to solve for each bit of the
encoded values for each key.

Arguments:
    key_signatures: An iterable collection of N unique signatures.
    values: An interable collection of N values corresponding to signatures
    encoded_values: An iterable collection of N bitarrays, representing the
        encoded values.
    seed: A seed for hashing.

    Returns:
            SparseSystemPtr to solve for each key's encoded bits.
*/
template <typename T>
SparseSystemPtr
constructModulo2System(const std::vector<__uint128_t> &key_signatures,
                       const std::vector<T> &values,
                       const CodeDict<T> &codedict, uint32_t max_codelength,
                       uint32_t seed, float DELTA) {
  uint64_t num_equations = 0;
  for (const auto &v : values) {
    num_equations += codedict.find(v)->second.numBits();
  }

  // TODO(david) should we add max_codelength to num_variables, was getting a
  // core dumped error
  uint64_t num_variables =
      std::ceil(static_cast<double>(num_equations) * DELTA);

  auto sparse_system =
      SparseSystem::make(num_equations, num_variables + max_codelength);

  uint64_t start_var_locations[3];
  for (uint32_t i = 0; i < key_signatures.size(); i++) {
    signatureToEquation(key_signatures[i], seed, num_variables,
                        start_var_locations);

    const BitArray &coded_value = codedict.find(values[i])->second;
    uint32_t n_bits = coded_value.numBits();
    for (uint32_t offset = 0; offset < n_bits; offset++) {
      uint32_t bit = coded_value[offset];
      sparse_system->addEquation(start_var_locations, offset, bit);
    }
  }

  return sparse_system;
}

template <typename T>
SubsystemSolutionSeedPair
constructAndSolveSubsystem(const std::vector<__uint128_t> &key_signatures,
                           const std::vector<T> &values,
                           const CodeDict<T> &codedict, uint32_t max_codelength,
                           float DELTA) {
  uint32_t seed = 0;
  uint32_t num_tries = 0;
  uint32_t max_num_attempts = 128;
  while (true) {
    try {
      SparseSystemPtr sparse_system = constructModulo2System<T>(
          key_signatures, values, codedict, max_codelength, seed, DELTA);

      BitArrayPtr solution = solveModulo2System(sparse_system);

      return {solution, seed};
    } catch (const UnsolvableSystemException &e) {
      seed++;
      num_tries++;

      if (num_tries == max_num_attempts) {
        throw std::runtime_error("Tried to solve system " +
                                 std::to_string(num_tries) +
                                 " times with no success.");
      }
    }
  }
}

// This is a constant multiplier on the number of variables based on the
// number of equations expected. This constant makes the system solvable
// with very high probability. If we want faster construction at the cost of
// 12% more memory, we can omit lazy gaussian elimination and set delta
// to 1.23. This delta also depends on the number of hashes we use per
// equation. This delta is for 3 hashes but for 4 it would be different.
static const double DELTA = 1.089;

/**
 * Constructs a Csf from the given keys and values.
 */
template <typename T>
CsfPtr<T>
constructCsf(const std::vector<std::string> &keys, const std::vector<T> &values,
             PreFilterConfigPtr filter_config = nullptr, bool verbose = true) {
  if (values.empty()) {
    throw std::invalid_argument("Values must be non-empty but found length 0.");
  }

  if (keys.size() != values.size()) {
    throw std::invalid_argument("Keys and values must have the same length.");
  }

  Timer timer;

  std::vector<std::string> filtered_keys = keys;
  std::vector<T> filtered_values = values;

  PreFilterPtr<T> filter = nullptr;
  if (filter_config) {
    filter = FilterFactory::makeFilter<T>(filter_config);
    filter->apply(filtered_keys, filtered_values, DELTA, verbose);
  }

  // If all keys were filtered out (all values were most common), create empty
  // CSF. Query will always go through filter and return most common value.
  if (filtered_keys.empty()) {
    std::vector<SubsystemSolutionSeedPair> empty_solutions;
    std::vector<uint32_t> empty_code_length_counts;
    std::vector<T> empty_ordered_symbols;
    return Csf<T>::make(empty_solutions, empty_code_length_counts,
                        empty_ordered_symbols, 0, filter);
  }

  if (verbose) {
    std::cout << "Creating codebook...";
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

  if (verbose) {
    std::cout << " finished in " << timer.seconds() << " seconds." << std::endl;
    std::cout << "Partitioning to buckets...";
  }

  BucketedHashStore<T> hash_store =
      partitionToBuckets<T>(filtered_keys, filtered_values, bucket_size);

  if (verbose) {
    std::cout << " finished in " << timer.seconds() << " seconds." << std::endl;
  }

  std::exception_ptr exception = nullptr;
  std::vector<SubsystemSolutionSeedPair> solutions_and_seeds(
      hash_store.num_buckets);
  auto bar = ProgressBar::makeOptional(verbose, "Solving systems...",
                                       /* max_steps=*/hash_store.num_buckets);

#pragma omp parallel for default(none)                                         \
    shared(hash_store, huffman, solutions_and_seeds, exception, bar, DELTA)
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
      { exception = std::current_exception(); }
    }
    if (bar) {
#pragma omp critical
      bar->increment();
    }
  }

  if (exception) {
    std::rethrow_exception(exception);
  }

  if (bar) {
    std::string str = "Solving systems...  finished in " +
                      std::to_string(timer.seconds()) + " seconds.\n";
    bar->close(str);
  }

  return Csf<T>::make(solutions_and_seeds, huffman.code_length_counts,
                      huffman.ordered_symbols, hash_store.seed, filter);
}

} // namespace caramel