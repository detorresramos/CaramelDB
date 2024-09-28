#pragma once

#include "BloomPrefiltering.h"
#include "BucketedHashStore.h"
#include "Codec.h"
#include "ConstructUtils.h"
#include "Csf.h"
#include <array>
#include <cmath>
#include <functional>
#include <src/Modulo2System.h>
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
    num_equations += codedict.find(v)->second->numBits();
  }

  uint64_t num_variables =
      std::ceil(static_cast<double>(num_equations) * DELTA);

  auto sparse_system =
      SparseSystem::make(num_equations, num_variables + max_codelength);

  uint64_t start_var_locations[3];
  for (uint32_t i = 0; i < key_signatures.size(); i++) {
    signatureToEquation(key_signatures[i], seed, num_variables,
                        start_var_locations);

    BitArrayPtr coded_value = codedict.find(values[i])->second;
    uint32_t n_bits = coded_value->numBits();
    for (uint32_t offset = 0; offset < n_bits; offset++) {
      uint32_t bit = (*coded_value)[offset];
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

/**
 * Constructs a Csf from the given keys and values.
 */
template <typename T>
CsfPtr<T> constructCsf(const std::vector<std::string> &keys,
                       const std::vector<T> &values,
                       bool use_bloom_filter = true, bool verbose = true) {
  if (values.empty()) {
    throw std::invalid_argument("Values must be non-empty but found length 0.");
  }

  if (keys.size() != values.size()) {
    throw std::invalid_argument("Keys and values must have the same length.");
  }

  // This is a constant multiplier on the number of variables based on the
  // number of equations expected. This constant makes the system solvable
  // with very high probability. If we want faster construction at the cost of
  // 12% more memory, we can omit lazy gaussian elimination and set delta
  // to 1.23. This delta also depends on the number of hashes we use per
  // equation. This delta is for 3 hashes but for 4 it would be different.
  double DELTA = 1.089;

  Timer timer;

  std::vector<std::string> filtered_keys = keys;
  std::vector<T> filtered_values = values;
  BloomFilterPtr bloom_filter = nullptr;
  std::optional<T> most_common_value = std::nullopt;

  if (use_bloom_filter) {
    auto [highest_frequency, computed_most_common_value] =
        highestFrequency(values);
    float highest_normalized_frequency = static_cast<float>(highest_frequency) /
                                         static_cast<float>(values.size());

    float error_rate = calculateErrorRate(
        /* alpha= */ highest_normalized_frequency, /* delta= */ DELTA);

    if (error_rate < 0.5 && error_rate != 0) {
      if (verbose) {
        std::cout << "Applying bloom pre-filtering...";
      }
      auto bloom_prefiltering_output = bloomPrefiltering(
          keys, values,
          /* highest_frequency= */ highest_frequency,
          /* error_rate= */ error_rate,
          /* most_common_value= */ computed_most_common_value);
      filtered_keys = std::move(std::get<0>(bloom_prefiltering_output));
      filtered_values = std::move(std::get<1>(bloom_prefiltering_output));
      bloom_filter = std::get<2>(bloom_prefiltering_output);
      most_common_value = std::get<3>(bloom_prefiltering_output);
      if (verbose) {
        std::cout << " finished in " << timer.seconds() << " seconds."
                  << std::endl;
      }
    }
  }

  if (verbose) {
    std::cout << "Creating codebook...";
  }

  auto huffman_output = cannonicalHuffman<T>(filtered_values);
  auto &codedict = std::get<0>(huffman_output);
  auto &code_length_counts = std::get<1>(huffman_output);
  auto &ordered_symbols = std::get<2>(huffman_output);
  uint32_t max_codelength = code_length_counts.size() - 1;

  if (verbose) {
    std::cout << " finished in " << timer.seconds() << " seconds." << std::endl;
    std::cout << "Partitioning to buckets...";
  }

  auto buckets = partitionToBuckets<T>(filtered_keys, filtered_values);
  auto &bucketed_key_signatures = std::get<0>(buckets);
  auto &bucketed_values = std::get<1>(buckets);
  uint32_t hash_store_seed = std::get<2>(buckets);

  if (verbose) {
    std::cout << " finished in " << timer.seconds() << " seconds." << std::endl;
  }

  std::exception_ptr exception = nullptr;
  uint32_t num_buckets = bucketed_key_signatures.size();
  std::vector<SubsystemSolutionSeedPair> solutions_and_seeds(num_buckets);
  auto bar = ProgressBar::makeOptional(verbose, "Solving systems...",
                                       /* max_steps=*/num_buckets);

// #pragma omp parallel for default(none)                                         
//     shared(bucketed_key_signatures, bucketed_values, codedict, max_codelength, 
//            solutions_and_seeds, num_buckets, exception, bar, DELTA)
  for (uint32_t i = 0; i < num_buckets; i++) {
    if (exception) {
      continue;
    }
    try {
      solutions_and_seeds[i] = constructAndSolveSubsystem<T>(
          bucketed_key_signatures[i], bucketed_values[i], codedict,
          max_codelength, DELTA);
    } catch (std::exception &e) {
// #pragma omp critical
      { exception = std::current_exception(); }
    }
    if (bar) {
// #pragma omp critical
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

  return Csf<T>::make(solutions_and_seeds, code_length_counts, ordered_symbols,
                      hash_store_seed, bloom_filter, most_common_value);
}

} // namespace caramel