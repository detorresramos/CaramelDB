#pragma once

#include "BucketedHashStore.h"
#include "CsfCodebook.h"
#include "ConstructUtils.h"
#include "Csf.h"
#include <cmath>
#include <src/Modulo2System.h>
#include <src/construct/filter/AutoFilterSelection.h>
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
// Number of equations in a subsystem: the total coded bit-length of its values.
// This sum is the single expensive codedict pass; for high-entropy data it is
// ~one DRAM-bound lookup per value, so callers that already have it (e.g. the
// arena size pass) should reuse it rather than recomputing.
template <typename T>
uint64_t subsystemNumEquations(const std::vector<T> &values,
                               const CodeDict<T> &codedict) {
  uint64_t num_equations = 0;
  for (const auto &v : values) {
    num_equations += codedict.find(v)->second.numBits();
  }
  return num_equations;
}

// Variables (solution columns) for a given equation count: equations scaled by
// DELTA. The single formula the solver and the size pass both use.
inline uint64_t numVariablesFromEquations(uint64_t num_equations, float DELTA) {
  return static_cast<uint64_t>(
      std::ceil(static_cast<double>(num_equations) * DELTA));
}

// The width of a subsystem's solution is fully determined by its values' code
// lengths (not the key contents and not the solve seed): the number of
// equations scaled by DELTA. Exposed so the arena layout can be sized before
// solving, from the same formula the solver uses.
template <typename T>
uint64_t subsystemNumVariables(const std::vector<T> &values,
                               const CodeDict<T> &codedict, float DELTA) {
  return numVariablesFromEquations(subsystemNumEquations<T>(values, codedict),
                                   DELTA);
}

// Total bits in a solved subsystem's BitArray: the variables plus the
// max_codelength slack the decode's bit-slice reads past the last variable.
inline uint32_t solutionBitsFromEquations(uint64_t num_equations,
                                          uint32_t max_codelength, float DELTA) {
  return static_cast<uint32_t>(
             numVariablesFromEquations(num_equations, DELTA)) +
         max_codelength;
}

template <typename T>
uint32_t subsystemSolutionBits(const std::vector<T> &values,
                               const CodeDict<T> &codedict,
                               uint32_t max_codelength, float DELTA) {
  return solutionBitsFromEquations(subsystemNumEquations<T>(values, codedict),
                                   max_codelength, DELTA);
}

template <typename T>
SparseSystemPtr
constructModulo2System(const std::vector<__uint128_t> &key_signatures,
                       const std::vector<T> &values,
                       const CodeDict<T> &codedict, uint32_t max_codelength,
                       uint32_t seed, float DELTA, uint64_t num_equations = 0) {
  // num_equations == 0 means "not precomputed". A non-empty bucket always has a
  // positive coded length, so the sentinel never collides with a real count.
  if (num_equations == 0) {
    num_equations = subsystemNumEquations<T>(values, codedict);
  }
  uint64_t num_variables = numVariablesFromEquations(num_equations, DELTA);

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
                           float DELTA, uint64_t num_equations = 0) {
  uint32_t seed = 0;
  uint32_t num_tries = 0;
  uint32_t max_num_attempts = 128;
  while (true) {
    try {
      SparseSystemPtr sparse_system = constructModulo2System<T>(
          key_signatures, values, codedict, max_codelength, seed, DELTA,
          num_equations);

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
static constexpr double DELTA = 1.089;

static constexpr uint64_t TARGET_EQUATIONS_PER_BUCKET = 3500;

// Temporary knob for the bucket-size sweep (smaller buckets = fewer cache
// lines touched per column at query time, but more per-bucket padding and
// metadata).
inline uint64_t targetEquationsPerBucket() {
  static const uint64_t value = []() {
    const char *s = getenv("CARAMEL_BUCKET_EQUATIONS");
    return s ? strtoull(s, nullptr, 10) : TARGET_EQUATIONS_PER_BUCKET;
  }();
  return value;
}

inline uint64_t bucketCountDivisor() {
  static const uint64_t value = []() {
    const char *s = getenv("CARAMEL_MAX_BUCKET_DIVISOR");
    return s ? strtoull(s, nullptr, 10) : 100;
  }();
  return value;
}

template <typename T>
uint64_t targetBucketCount(const std::vector<T> &values,
                           const CodeDict<T> &codedict) {
  uint64_t total_equations = 0;
  for (const auto &v : values) {
    total_equations += codedict.at(v).numBits();
  }
  uint64_t num_keys = values.size();
  return std::clamp(
      total_equations / targetEquationsPerBucket(),
      num_keys / 1000 + 1,
      num_keys / bucketCountDivisor() + 1);
}

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

  std::vector<std::string> filtered_keys_storage;
  std::vector<T> filtered_values_storage;

  PreFilterPtr<T> filter = nullptr;
  if (filter_config) {
    auto actual_config = filter_config;
    if (std::dynamic_pointer_cast<AutoPreFilterConfig>(filter_config)) {
      actual_config = selectBestFilter<T>(values, verbose);
    }
    if (actual_config) {
      filter = FilterFactory::makeFilter<T>(actual_config);
      filter->apply(keys, values, filtered_keys_storage,
                    filtered_values_storage, DELTA, verbose);
    }
  }

  const auto &active_keys = filter ? filtered_keys_storage : keys;
  const auto &active_values = filter ? filtered_values_storage : values;

  // If all keys were filtered out (all values were most common), create empty
  // CSF. Query will always go through filter and return most common value.
  if (active_keys.empty()) {
    std::vector<SubsystemSolutionSeedPair> empty_solutions;
    std::vector<uint32_t> empty_code_length_counts;
    std::vector<T> empty_ordered_symbols;
    return Csf<T>::make(empty_solutions, empty_code_length_counts,
                        empty_ordered_symbols, 0, filter);
  }

  if (verbose) {
    std::cout << "Creating codebook...";
  }

  CsfCodebook<T> codebook = canonicalHuffman<T>(active_values);

  uint64_t num_buckets = targetBucketCount(active_values, codebook.codedict);

  if (verbose) {
    std::cout << " finished in " << timer.seconds() << " seconds." << std::endl;
    std::cout << "Partitioning to buckets...";
  }

  BucketedHashStore<T> hash_store =
      partitionToBuckets<T>(active_keys, active_values, num_buckets);

  if (verbose) {
    std::cout << " finished in " << timer.seconds() << " seconds." << std::endl;
  }

  std::exception_ptr exception = nullptr;
  std::vector<SubsystemSolutionSeedPair> solutions_and_seeds(
      hash_store.num_buckets);
  auto bar = ProgressBar::makeOptional(verbose, "Solving systems...",
                                       /* max_steps=*/hash_store.num_buckets);

#pragma omp parallel for default(none)                                         \
    shared(hash_store, codebook, solutions_and_seeds, exception, bar, DELTA)
  for (uint32_t i = 0; i < hash_store.num_buckets; i++) {
    if (exception) {
      continue;
    }
    try {
      solutions_and_seeds[i] = constructAndSolveSubsystem<T>(
          hash_store.key_buckets[i], hash_store.value_buckets[i],
          codebook.codedict, codebook.max_codelength, DELTA);
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

  return Csf<T>::make(solutions_and_seeds, codebook.code_length_counts,
                      codebook.ordered_symbols, hash_store.seed, filter);
}

} // namespace caramel