#pragma once

#include "BucketedHashStore.h"
#include "Codec.h"
#include "ConstructUtils.h"
#include "CsfStats.h"
#include "filter/BinaryFusePreFilter.h"
#include "filter/BloomPreFilter.h"
#include "filter/XORPreFilter.h"
#include <cereal/access.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/vector.hpp>
#include <limits>
#include <memory>
#include <src/BitArray.h>
#include <src/construct/filter/PreFilter.h>
#include <src/utils/SafeFileIO.h>
#include <vector>

namespace caramel {

template <typename T> class Csf;

template <typename T> using CsfPtr = std::shared_ptr<Csf<T>>;

using SubsystemSolutionSeedPair = std::pair<BitArrayPtr, uint32_t>;

class CsfDeserializationException : public std::runtime_error {
public:
  explicit CsfDeserializationException(const std::string &message)
      : std::runtime_error("Cannot deserialize CSF: " + message) {};
};

template <typename T> class Csf {
public:
  Csf(const std::vector<SubsystemSolutionSeedPair> &solutions_and_seeds,
      const std::vector<uint32_t> &code_length_counts,
      const std::vector<T> &ordered_symbols, uint32_t hash_store_seed,
      const PreFilterPtr<T> &filter)
      : _solutions_and_seeds(solutions_and_seeds),
        _code_length_counts(code_length_counts),
        _ordered_symbols(ordered_symbols), _hash_store_seed(hash_store_seed),
        _filter(filter), _max_codelength(_code_length_counts.size() - 1) {}

  static CsfPtr<T>
  make(const std::vector<SubsystemSolutionSeedPair> &solutions_and_seeds,
       const std::vector<uint32_t> &code_length_counts,
       const std::vector<T> &ordered_symbols, uint32_t hash_store_seed,
       const PreFilterPtr<T> &filter) {
    return std::make_shared<Csf<T>>(solutions_and_seeds, code_length_counts,
                                    ordered_symbols, hash_store_seed, filter);
  }

  T query(const std::string &key) const {
    if (_filter && !_filter->contains(key)) {
      return *_filter->getMostCommonValue();
    }

    // If CSF has no solutions (all values were filtered), return most common
    if (_solutions_and_seeds.empty()) {
      if (_filter && _filter->getMostCommonValue()) {
        return *_filter->getMostCommonValue();
      }
      throw std::runtime_error("Cannot query empty CSF without filter");
    }

    __uint128_t signature = hashKey(key, _hash_store_seed);

    uint32_t bucket_id =
        getBucketID(signature, /* num_buckets= */ _solutions_and_seeds.size());

    auto &[solution, construction_seed] = _solutions_and_seeds.at(bucket_id);

    uint32_t solution_size = solution->numBits();

    uint64_t e[3];
    signatureToEquation(signature, construction_seed,
                        solution_size - _max_codelength, e);

    uint64_t encoded_value = solution->getuint64(e[0], _max_codelength) ^
                             solution->getuint64(e[1], _max_codelength) ^
                             solution->getuint64(e[2], _max_codelength);

    return cannonicalDecodeFromNumber(encoded_value, _code_length_counts,
                                      _ordered_symbols, _max_codelength);
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

  PreFilterPtr<T> getFilter() const { return _filter; }

  CsfStats getStats() const {
    CsfStats stats;

    // Bucket statistics
    stats.bucket_stats.num_buckets = _solutions_and_seeds.size();
    stats.bucket_stats.total_solution_bits = 0;
    stats.bucket_stats.min_solution_bits = std::numeric_limits<size_t>::max();
    stats.bucket_stats.max_solution_bits = 0;

    for (const auto &[solution, seed] : _solutions_and_seeds) {
      size_t bits = solution->numBits();
      stats.bucket_stats.total_solution_bits += bits;
      stats.bucket_stats.min_solution_bits =
          std::min(stats.bucket_stats.min_solution_bits, bits);
      stats.bucket_stats.max_solution_bits =
          std::max(stats.bucket_stats.max_solution_bits, bits);
    }

    stats.bucket_stats.avg_solution_bits =
        stats.bucket_stats.num_buckets > 0
            ? static_cast<double>(stats.bucket_stats.total_solution_bits) /
                  stats.bucket_stats.num_buckets
            : 0;
    if (stats.bucket_stats.num_buckets == 0)
      stats.bucket_stats.min_solution_bits = 0;

    // Huffman statistics
    stats.huffman_stats.num_unique_symbols = _ordered_symbols.size();
    stats.huffman_stats.max_code_length = _max_codelength;
    stats.huffman_stats.code_length_distribution = _code_length_counts;

    size_t total_symbols = 0, total_bits = 0;
    for (size_t len = 1; len < _code_length_counts.size(); len++) {
      total_symbols += _code_length_counts[len];
      total_bits += _code_length_counts[len] * len;
    }
    stats.huffman_stats.avg_bits_per_symbol =
        total_symbols > 0 ? static_cast<double>(total_bits) / total_symbols
                          : 0.0;

    // Solution memory (bits to bytes, rounding up to 64-bit blocks)
    size_t solution_bytes = 0;
    for (const auto &[solution, seed] : _solutions_and_seeds) {
      uint32_t num_blocks = ((solution->numBits() - 1) / 64) + 1;
      solution_bytes += num_blocks * sizeof(uint64_t);
    }
    stats.solution_bytes = static_cast<double>(solution_bytes);

    // Filter statistics
    stats.filter_bytes = 0;
    if (_filter) {
      stats.filter_stats = getFilterStats();
      if (stats.filter_stats) {
        stats.filter_bytes =
            static_cast<double>(stats.filter_stats->size_bytes);
      }
    }

    // Metadata bytes
    size_t metadata_bytes = 0;
    metadata_bytes += _code_length_counts.size() * sizeof(uint32_t);
    metadata_bytes += _ordered_symbols.size() * sizeof(T);
    metadata_bytes += sizeof(_hash_store_seed) + sizeof(_max_codelength);
    // Per-bucket: seed (4) + BitArray serialization framing (13):
    //   shared_ptr id (4) + _num_bits (4) + _num_blocks (4) + _owns_data (1)
    metadata_bytes += _solutions_and_seeds.size() * (sizeof(uint32_t) + 13);
    stats.metadata_bytes = static_cast<double>(metadata_bytes);

    stats.in_memory_bytes = static_cast<size_t>(
        stats.solution_bytes + stats.filter_bytes + stats.metadata_bytes);

    return stats;
  }

private:
  std::optional<FilterStats> getFilterStats() const {
    if (!_filter)
      return std::nullopt;

    FilterStats fs;

    if (auto bloom = std::dynamic_pointer_cast<BloomPreFilter<T>>(_filter)) {
      auto bf = bloom->getBloomFilter();
      fs.type = "bloom";
      if (bf) {
        fs.size_bits = bf->size();
        fs.size_bytes = (bf->size() + 7) / 8;
        fs.num_hashes = bf->numHashes();
      } else {
        fs.size_bytes = 0;
      }
      fs.num_elements = 0;
    } else if (auto xor_pf =
                   std::dynamic_pointer_cast<XORPreFilter<T>>(_filter)) {
      auto xf = xor_pf->getXorFilter();
      fs.type = "xor";
      if (xf) {
        fs.size_bytes = xf->size();
        fs.num_elements = xf->numElements();
        fs.fingerprint_bits = xf->fingerprintWidth();
      } else {
        fs.size_bytes = 0;
        fs.num_elements = 0;
      }
    } else if (auto bf_pf =
                   std::dynamic_pointer_cast<BinaryFusePreFilter<T>>(_filter)) {
      auto bff = bf_pf->getBinaryFuseFilter();
      fs.type = "binary_fuse";
      if (bff) {
        fs.size_bytes = bff->size();
        fs.num_elements = bff->numElements();
        fs.fingerprint_bits = bff->fingerprintWidth();
      } else {
        fs.size_bytes = 0;
        fs.num_elements = 0;
      }
    } else {
      fs.type = "unknown";
      fs.size_bytes = 0;
      fs.num_elements = 0;
    }

    return fs;
  }

  // Private constructor for cereal
  Csf() {}

  friend class cereal::access;
  template <class Archive> void serialize(Archive &archive) {
    archive(_solutions_and_seeds, _code_length_counts, _ordered_symbols,
            _hash_store_seed, _filter, _max_codelength);
  }

  std::vector<SubsystemSolutionSeedPair> _solutions_and_seeds;
  std::vector<uint32_t> _code_length_counts;
  std::vector<T> _ordered_symbols;
  uint32_t _hash_store_seed;
  PreFilterPtr<T> _filter = nullptr;
  uint32_t _max_codelength;
};

} // namespace caramel