#include "Construct.h"
#include "BucketedHashStore.h"
#include "Codec.h"
#include "ConstructUtils.h"
#include "SpookyHash.h"
#include <src/solve/Solve.h>

namespace caramel {

CsfPtr constructCsf(const std::vector<std::string> &keys,
                    const std::vector<uint32_t> &values) {
  auto huffman_output = cannonicalHuffman(values);
  auto &codedict = std::get<0>(huffman_output);
  auto &code_length_counts = std::get<1>(huffman_output);
  auto &ordered_symbols = std::get<2>(huffman_output);

  auto buckets = partitionToBuckets(keys, values);
  auto &bucketed_key_signatures = std::get<0>(buckets);
  auto &bucketed_values = std::get<1>(buckets);
  uint32_t hash_store_seed = std::get<2>(buckets);

  uint32_t num_buckets = bucketed_key_signatures.size();

  std::vector<SubsystemSolutionSeedPair> solutions_and_seeds(num_buckets);

#pragma omp parallel for default(none)                                         \
    shared(bucketed_key_signatures, bucketed_values, codedict,                 \
           solutions_and_seeds, num_buckets)
  for (uint32_t i = 0; i < num_buckets; i++) {
    solutions_and_seeds[i] = constructAndSolveSubsystem(
        bucketed_key_signatures[i], bucketed_values[i], codedict);
  }

  return Csf::make(solutions_and_seeds, code_length_counts, ordered_symbols,
                   hash_store_seed);
}

SubsystemSolutionSeedPair
constructAndSolveSubsystem(const std::vector<Uint128Signature> &key_signatures,
                           const std::vector<uint32_t> &values,
                           const CodeDict &codedict) {
  uint32_t seed = 0;
  uint32_t num_tries = 0;
  uint32_t max_num_attempts = 10;
  while (true) {
    try {
      SparseSystemPtr sparse_system =
          constructModulo2System(key_signatures, values, codedict, seed);

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

SparseSystemPtr
constructModulo2System(const std::vector<Uint128Signature> &key_signatures,
                       const std::vector<uint32_t> &values,
                       const CodeDict &codedict, uint32_t seed) {
  // This is a constant multiplier on the number of variables based on the
  // number of equations expected. This constant makes the system solvable
  // with very high probability. If we want faster construction at the cost of
  // 12% more memory, we can omit lazy gaussian elimination and set delta
  // to 1.23
  double DELTA = 1.10;

  uint32_t num_equations = 0;
  for (const auto &v : values) {
    num_equations += codedict.at(v)->numBits();
  }

  uint32_t num_variables =
      std::ceil(static_cast<double>(num_equations) * DELTA);

  auto sparse_system = SparseSystem::make(num_variables);

  uint32_t equation_id = 0;
  for (uint32_t i = 0; i < key_signatures.size(); i++) {
    Uint128Signature key_signature = key_signatures.at(i);

    std::vector<uint32_t> start_var_locations =
        getStartVarLocations(key_signature, seed, num_variables);

    BitArrayPtr coded_value = codedict.at(values.at(i));
    for (uint32_t offset = 0; offset < coded_value->numBits(); offset++) {
      uint32_t bit = (*coded_value)[offset];
      std::vector<uint32_t> participating_variables;
      for (uint32_t start_var_location : start_var_locations) {
        uint32_t var_location = start_var_location + offset;
        if (var_location >= num_variables) {
          var_location -= num_variables;
        }
        participating_variables.push_back(var_location);
      }
      sparse_system->addEquation(equation_id, participating_variables, bit);
      equation_id++;
    }
  }

  return sparse_system;
}

} // namespace caramel