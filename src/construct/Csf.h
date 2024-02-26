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
#include <cereal/types/optional.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/vector.hpp>
#include <chrono>
#include <memory>
#include <src/BitArray.h>
#include <src/construct/BloomFilter.h>
#include <src/utils/SafeFileIO.h>
#include <vector>

namespace caramel {

template <typename T> class Csf;

template <typename T> using CsfPtr = std::shared_ptr<Csf<T>>;

using SubsystemSolutionSeedPair = std::pair<BitArrayPtr, uint32_t>;

class CsfDeserializationException : public std::runtime_error {
public:
  explicit CsfDeserializationException(const std::string &message)
      : std::runtime_error("Cannot deserialize CSF: " + message){};
};

template <typename T> class Csf {
public:
  Csf(const std::vector<SubsystemSolutionSeedPair> &solutions_and_seeds,
      const std::vector<uint32_t> &code_length_counts,
      const std::vector<T> &ordered_symbols, uint32_t hash_store_seed,
      const BloomFilterPtr &bloom_filter, std::optional<T> most_common_value)
      : _solutions_and_seeds(solutions_and_seeds),
        _code_length_counts(code_length_counts),
        _ordered_symbols(ordered_symbols), _hash_store_seed(hash_store_seed),
        _bloom_filter(bloom_filter), _most_common_value(most_common_value) {
    if ((_bloom_filter != nullptr) != (_most_common_value.has_value())) {
      throw std::invalid_argument("If using bloom filter must provide both the "
                                  "bloom filter and the most common value.");
    }
  }

  static CsfPtr<T>
  make(const std::vector<SubsystemSolutionSeedPair> &solutions_and_seeds,
       const std::vector<uint32_t> &code_length_counts,
       const std::vector<T> &ordered_symbols, uint32_t hash_store_seed,
       const BloomFilterPtr bloom_filter, std::optional<T> most_common_value) {
    return std::make_shared<Csf<T>>(solutions_and_seeds, code_length_counts,
                                    ordered_symbols, hash_store_seed,
                                    bloom_filter, most_common_value);
  }

  T query(const std::string &key) const {
    if (_bloom_filter && !_bloom_filter->contains(key)) {
      return *_most_common_value;
    }

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

  static std::vector<T> parallelInference(const std::string &key,
                                          const std::vector<CsfPtr<T>> &csfs) {
    std::vector<T> outputs(csfs.size());

#pragma omp parallel for default(none) shared(key, csfs, outputs)
    for (size_t i = 0; i < csfs.size(); i++) {
      outputs[i] = csfs[i]->query(key);
    }

    return outputs;
  }

  void save(const std::string &filename, const uint32_t type_id = 0) const {
    auto output_stream = SafeFileIO::ofstream(filename, std::ios::binary);
    output_stream.write(reinterpret_cast<const char *>(&type_id),
                        sizeof(uint32_t));
    cereal::BinaryOutputArchive oarchive(output_stream);
    oarchive(*this);
  }

  static CsfPtr<T> load(const std::string &filename,
                        const uint32_t type_id = 0) {
    auto input_stream = SafeFileIO::ifstream(filename, std::ios::binary);
    // Check the type_id before deserializing a (potentially large) CSF.
    uint32_t type_id_found = 0;
    input_stream.read(reinterpret_cast<char *>(&type_id_found),
                      sizeof(uint32_t));
    if (type_id != type_id_found) {
      throw CsfDeserializationException(
          "Expected type_id to be " + std::to_string(type_id) +
          " but found type_id = " + std::to_string(type_id_found) +
          " when deserializing " + filename);
    }
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
            _hash_store_seed, _bloom_filter, _most_common_value);
  }

  std::vector<SubsystemSolutionSeedPair> _solutions_and_seeds;
  std::vector<uint32_t> _code_length_counts;
  std::vector<T> _ordered_symbols;
  uint32_t _hash_store_seed;
  BloomFilterPtr _bloom_filter;
  std::optional<T> _most_common_value;
};

} // namespace caramel