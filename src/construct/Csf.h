#pragma once
#include "BucketedHashStore.h"
#include "Codec.h"
#include "ConstructUtils.h"
#include "Csf.h"
#include "SpookyHash.h"
#include <cereal/access.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/vector.hpp>
#include <memory>
#include <src/BitArray.h>
#include <src/utils/SafeFileIO.h>
#include <vector>

namespace caramel {

template <typename T> class Csf;

template <typename T> using CsfPtr = std::shared_ptr<Csf<T>>;

using SubsystemSolutionSeedPair = std::pair<BitArrayPtr, uint32_t>;

template <typename T> class Csf {
public:
  Csf(const std::vector<SubsystemSolutionSeedPair> &solutions_and_seeds,
      const std::vector<uint32_t> &code_length_counts,
      const std::vector<T> &ordered_symbols, uint32_t hash_store_seed)
      : _solutions_and_seeds(solutions_and_seeds),
        _code_length_counts(code_length_counts),
        _ordered_symbols(ordered_symbols), _hash_store_seed(hash_store_seed) {}

  static CsfPtr<T>
  make(const std::vector<SubsystemSolutionSeedPair> &solutions_and_seeds,
       const std::vector<uint32_t> &code_length_counts,
       const std::vector<T> &ordered_symbols, uint32_t hash_store_seed) {
    return std::make_shared<Csf<T>>(solutions_and_seeds, code_length_counts,
                                    ordered_symbols, hash_store_seed);
  }

  T query(const std::string &key) const {
    Uint128Signature signature = hashKey(key, _hash_store_seed);

    uint32_t bucket_id =
        getBucketID(signature, /* num_buckets= */ _solutions_and_seeds.size());

    auto [solution, construction_seed] = _solutions_and_seeds.at(bucket_id);

    uint32_t solution_size = solution->numBits();

    uint32_t max_codelength = _code_length_counts.size() - 1;

    std::vector<uint32_t> start_var_locations =
        getStartVarLocations(signature, construction_seed, solution_size);

    BitArrayPtr encoded_value = BitArray::make(max_codelength);
    for (auto location : start_var_locations) {
      BitArrayPtr temp = BitArray::make(max_codelength);
      for (uint32_t i = 0; i < max_codelength; i++) {
        if ((*solution)[location]) {
          temp->setBit(i);
        }
        if (location == solution_size - 1) {
          location = 0;
        } else {
          location++;
        }
      }
      *encoded_value ^= *temp;
    }

    return cannonicalDecode(encoded_value, _code_length_counts,
                            _ordered_symbols);
  }

  void save(const std::string &filename) const {
    auto output_stream = SafeFileIO::ofstream(filename, std::ios::binary);
    cereal::BinaryOutputArchive oarchive(output_stream);
    oarchive(*this);
  }

  static CsfPtr<T> load(const std::string &filename) {
    auto input_stream = SafeFileIO::ifstream(filename, std::ios::binary);
    cereal::BinaryInputArchive iarchive(input_stream);
    CsfPtr<T> deserialize_into(new Csf<T>());
    iarchive(*deserialize_into);

    return deserialize_into;
  }

  uint32_t size() const { return 0; }

private:
  // Private constructor for cereal
  Csf() {}

  friend class cereal::access;
  template <class Archive> void serialize(Archive &archive) {
    archive(_solutions_and_seeds, _code_length_counts, _ordered_symbols,
            _hash_store_seed);
  }

  std::vector<SubsystemSolutionSeedPair> _solutions_and_seeds;
  std::vector<uint32_t> _code_length_counts;
  std::vector<T> _ordered_symbols;
  uint32_t _hash_store_seed;
};

} // namespace caramel