#pragma once

#include "src/construct/Construct.h"
#include "src/construct/multiset/PackedCsf.h"
#include <limits>

namespace caramel {

// Builds a sparse linear system from pre-encoded BitArrays (one per key).
inline SparseSystemPtr constructModulo2SystemFromBitArrays(
    const std::vector<__uint128_t> &key_signatures,
    const std::vector<BitArrayPtr> &encoded_values, uint32_t max_total_bits,
    uint32_t seed, float DELTA) {

  uint64_t num_equations = 0;
  for (const auto &ev : encoded_values) {
    num_equations += ev->numBits();
  }

  uint64_t num_variables =
      std::ceil(static_cast<double>(num_equations) * DELTA);

  auto sparse_system =
      SparseSystem::make(num_equations, num_variables + max_total_bits);

  uint64_t start_var_locations[3];
  for (uint32_t i = 0; i < key_signatures.size(); i++) {
    signatureToEquation(key_signatures[i], seed, num_variables,
                        start_var_locations);

    const BitArray &ev = *encoded_values[i];
    uint32_t n_bits = ev.numBits();
    for (uint32_t offset = 0; offset < n_bits; offset++) {
      uint32_t bit = ev[offset];
      sparse_system->addEquation(start_var_locations, offset, bit);
    }
  }

  return sparse_system;
}

inline SubsystemSolutionSeedPair constructAndSolveSubsystemFromBitArrays(
    const std::vector<__uint128_t> &key_signatures,
    const std::vector<BitArrayPtr> &encoded_values, uint32_t max_total_bits,
    float DELTA) {
  uint32_t seed = 0;
  uint32_t max_num_attempts = 128;
  while (true) {
    try {
      SparseSystemPtr sparse_system = constructModulo2SystemFromBitArrays(
          key_signatures, encoded_values, max_total_bits, seed, DELTA);
      BitArrayPtr solution = solveModulo2System(sparse_system);
      return {solution, seed};
    } catch (const UnsolvableSystemException &) {
      seed++;
      if (seed == max_num_attempts) {
        throw std::runtime_error("Tried to solve system " +
                                 std::to_string(max_num_attempts) +
                                 " times with no success.");
      }
    }
  }
}

struct BucketedBitArrayStore {
  std::vector<std::vector<__uint128_t>> key_buckets;
  std::vector<std::vector<BitArrayPtr>> encoded_value_buckets;
  uint64_t seed;
  uint64_t num_buckets;
};

inline BucketedBitArrayStore
partitionToBucketsWithBitArrays(const std::vector<std::string> &keys,
                                const std::vector<BitArrayPtr> &encoded_values,
                                uint64_t num_buckets,
                                uint32_t num_attempts = 3) {
  uint64_t approximate_bucket_size = keys.size() / num_buckets + 1;

  for (uint64_t seed = 0; seed < num_attempts; seed++) {
    std::vector<std::vector<__uint128_t>> key_buckets(num_buckets);
    std::vector<std::vector<BitArrayPtr>> value_buckets(num_buckets);

    for (size_t i = 0; i < num_buckets; i++) {
      key_buckets[i].reserve(approximate_bucket_size);
      value_buckets[i].reserve(approximate_bucket_size);
    }

    for (size_t i = 0; i < keys.size(); i++) {
      __uint128_t signature = hashKey(keys[i], seed);
      uint32_t bucket_id = getBucketID(signature, num_buckets);
      key_buckets[bucket_id].push_back(signature);
      value_buckets[bucket_id].push_back(encoded_values[i]);
    }

    bool has_collision = false;
    std::exception_ptr exception = nullptr;
#pragma omp parallel for default(none)                                         \
    shared(num_buckets, key_buckets, has_collision, exception)
    for (size_t i = 0; i < num_buckets; i++) {
      const auto &bucket = key_buckets[i];
      std::unordered_set<__uint128_t> uniques(bucket.begin(), bucket.end());
      if (uniques.size() != bucket.size()) {
#pragma omp critical
        {
          has_collision = true;
          exception = std::make_exception_ptr(
              std::runtime_error("Detected a key collision under 128-bit hash. "
                                 "Likely due to a duplicate key."));
        }
      }
    }

    if (!has_collision) {
      return {std::move(key_buckets), std::move(value_buckets), seed,
              num_buckets};
    }

    if (seed == num_attempts - 1 && exception) {
      std::rethrow_exception(exception);
    }
  }

  throw std::invalid_argument("Fatal error: should never reach here.");
}

template <typename T>
PackedCsfPtr<T>
constructPackedCsf(const std::vector<std::string> &keys,
                   const std::vector<std::vector<T>> &values,
                   bool verbose = true) {

  T stop_symbol = std::numeric_limits<T>::max();

  // Build codebook from all values + STOP symbol
  std::vector<T> all_values;
  for (const auto &row : values) {
    all_values.insert(all_values.end(), row.begin(), row.end());
  }
  for (size_t i = 0; i < keys.size(); i++) {
    all_values.push_back(stop_symbol);
  }

  auto codebook =
      std::make_shared<CsfCodebook<T>>(canonicalHuffman<T>(all_values));

  // Concatenate Huffman codes for each key's values + STOP
  std::vector<BitArrayPtr> encoded_values(keys.size());
  uint32_t max_total_bits = 0;

  for (size_t i = 0; i < keys.size(); i++) {
    uint32_t total_bits = 0;
    for (const auto &v : values[i]) {
      total_bits += codebook->codedict.at(v).numBits();
    }
    total_bits += codebook->codedict.at(stop_symbol).numBits();
    max_total_bits = std::max(max_total_bits, total_bits);
  }

  // Add max_codelength padding to max_total_bits so the query can safely
  // extract max_codelength bits at any position (needed when the last
  // symbol's code is shorter than max_codelength).
  uint32_t max_cl = codebook->max_codelength;
  max_total_bits += max_cl;

  // Encode each key's values as a concatenated bitstring. No padding —
  // each key's BitArray is only as long as its actual encoded content.
  // The STOP symbol tells the decoder when to stop; shorter keys simply
  // contribute fewer equations to the linear system.
  for (size_t i = 0; i < keys.size(); i++) {
    uint32_t total_bits = 0;
    for (const auto &v : values[i]) {
      total_bits += codebook->codedict.at(v).numBits();
    }
    total_bits += codebook->codedict.at(stop_symbol).numBits();

    auto encoded = std::make_shared<BitArray>(total_bits);
    uint32_t pos = 0;
    for (const auto &v : values[i]) {
      const auto &code = codebook->codedict.at(v);
      for (uint32_t b = 0; b < code.numBits(); b++) {
        if (code[b])
          encoded->setBit(pos + b);
      }
      pos += code.numBits();
    }
    const auto &stop_code = codebook->codedict.at(stop_symbol);
    for (uint32_t b = 0; b < stop_code.numBits(); b++) {
      if (stop_code[b])
        encoded->setBit(pos + b);
    }

    encoded_values[i] = encoded;
  }

  if (verbose) {
    std::cout << "Packed CSF: " << keys.size() << " keys, max_total_bits="
              << max_total_bits << std::endl;
  }

  // Compute target bucket count. Packed CSFs have many more equations per key
  // (max_total_bits vs max_codelength), so we need more buckets than standard
  // CSFs to keep per-bucket system size manageable.
  uint64_t total_equations = 0;
  for (const auto &ev : encoded_values) {
    total_equations += ev->numBits();
  }
  uint64_t num_keys = keys.size();
  uint64_t num_buckets = std::max(
      total_equations / TARGET_EQUATIONS_PER_BUCKET, num_keys / 1000 + 1);

  BucketedBitArrayStore hash_store =
      partitionToBucketsWithBitArrays(keys, encoded_values, num_buckets);

  std::exception_ptr exception = nullptr;
  std::vector<SubsystemSolutionSeedPair> solutions_and_seeds(
      hash_store.num_buckets);

#pragma omp parallel for default(none)                                         \
    shared(hash_store, solutions_and_seeds, exception, max_total_bits, DELTA)
  for (uint32_t j = 0; j < hash_store.num_buckets; j++) {
    if (exception) {
      continue;
    }
    try {
      solutions_and_seeds[j] = constructAndSolveSubsystemFromBitArrays(
          hash_store.key_buckets[j], hash_store.encoded_value_buckets[j],
          max_total_bits, DELTA);
    } catch (std::exception &) {
#pragma omp critical
      { exception = std::current_exception(); }
    }
  }

  if (exception) {
    std::rethrow_exception(exception);
  }

  return std::make_shared<PackedCsf<T>>(std::move(solutions_and_seeds),
                                        hash_store.seed, codebook,
                                        max_total_bits, stop_symbol);
}

} // namespace caramel
