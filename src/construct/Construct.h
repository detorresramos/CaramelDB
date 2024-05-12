#pragma once

#include "BloomPrefiltering.h"
#include "BucketedHashStore.h"
#include "Codec.h"
#include "ConstructUtils.h"
#include "Csf.h"
#include "SpookyHash.h"
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
constructModulo2System(const std::vector<Uint128Signature> &key_signatures,
                       const std::vector<T> &values,
                       const CodeDict<T> &codedict, uint32_t seed,
                       float DELTA) {
  uint32_t num_equations = 0;
  for (const auto &v : values) {
    num_equations += codedict.find(v)->second->numBits();
  }

  uint32_t num_variables =
      std::ceil(static_cast<double>(num_equations) * DELTA);

  std::vector<std::vector<uint32_t>> equations;
  equations.reserve(num_equations);
  std::vector<uint32_t> constants;
  constants.reserve(num_equations);

  for (uint32_t i = 0; i < key_signatures.size(); i++) {
    Uint128Signature key_signature = key_signatures[i];

    std::vector<uint32_t> start_var_locations =
        signatureToEquation(key_signature, seed, num_variables);

    BitArrayPtr coded_value = codedict.find(values[i])->second;
    uint32_t n_bits = coded_value->numBits();
    for (uint32_t offset = 0; offset < n_bits; offset++) {
      uint32_t bit = (*coded_value)[offset];
      std::vector<uint32_t> participating_variables;
      for (uint32_t start_var_location : start_var_locations) {
        uint32_t var_location = start_var_location + offset;
        if (var_location >= num_variables) {
          var_location -= num_variables;
        }
        participating_variables.push_back(var_location);
      }
      equations.emplace_back(std::move(participating_variables));
      constants.emplace_back(bit);
    }
  }

  auto sparse_system = SparseSystem::make(std::move(equations),
                                          std::move(constants), num_variables);

  return sparse_system;
}

template <typename T>
SubsystemSolutionSeedPair
constructAndSolveSubsystem(const std::vector<Uint128Signature> &key_signatures,
                           const std::vector<T> &values,
                           const CodeDict<T> &codedict, float DELTA) {
  uint32_t seed = 0;
  uint32_t num_tries = 0;
  uint32_t max_num_attempts = 128;
  while (true) {
    try {
      SparseSystemPtr sparse_system = constructModulo2System<T>(
          key_signatures, values, codedict, seed, DELTA);

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
                       const std::vector<T> &values, bool use_bloom_filter = true,
                       bool verbose = true) {
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
  double DELTA = 1.10;

  Timer timer;

  if (verbose) {
    std::cout << "Applying bloom pre-filtering...";
  }

  std::vector<std::string> filtered_keys = keys;
  std::vector<T> filtered_values = values;
  BloomFilterPtr bloom_filter = nullptr;
  std::optional<T> most_common_value = std::nullopt;

  if (use_bloom_filter) {
    auto bloom_prefiltering_output = bloomPrefiltering(keys, values, DELTA);
    filtered_keys = std::move(std::get<0>(bloom_prefiltering_output));
    filtered_values = std::move(std::get<1>(bloom_prefiltering_output));
    bloom_filter = std::get<2>(bloom_prefiltering_output);
    most_common_value = std::get<3>(bloom_prefiltering_output);
  }

  if (verbose) {
    std::cout << " finished in " << timer.seconds() << " seconds." << std::endl;
    std::cout << "Creating codebook...";
  }

  auto huffman_output = cannonicalHuffman<T>(filtered_values);
  auto &codedict = std::get<0>(huffman_output);
  auto &code_length_counts = std::get<1>(huffman_output);
  auto &ordered_symbols = std::get<2>(huffman_output);

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

#pragma omp parallel for default(none)                                         \
    shared(bucketed_key_signatures, bucketed_values, codedict,                 \
           solutions_and_seeds, num_buckets, exception, bar, DELTA)
  for (uint32_t i = 0; i < num_buckets; i++) {
    if (exception) {
      continue;
    }
    try {
      solutions_and_seeds[i] = constructAndSolveSubsystem<T>(
          bucketed_key_signatures[i], bucketed_values[i], codedict, DELTA);
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

  return Csf<T>::make(solutions_and_seeds, code_length_counts, ordered_symbols,
                      hash_store_seed, bloom_filter, most_common_value);
}

} // namespace caramel