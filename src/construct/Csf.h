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
#include <chrono>
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

struct BucketQueryInfo {
  const uint64_t *data;
  uint32_t num_variables;
  uint32_t seed;
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
        _filter(filter), _max_codelength(_code_length_counts.size() - 1) {
    _buildQueryCache();
  }

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
    return _queryCore(key.data(), key.size());
  }

  T query(const char *data, size_t length) const {
    if (_filter && !_filter->contains(data, length)) {
      return *_filter->getMostCommonValue();
    }
    return _queryCore(data, length);
  }

  std::pair<std::vector<T>, double>
  benchmarkQueries(const std::vector<std::string> &keys,
                   uint32_t num_iterations) const {
    std::vector<T> results(keys.size());
    // Warmup
    for (const auto &key : keys) {
      query(key);
    }

    auto start = std::chrono::high_resolution_clock::now();
    for (uint32_t iter = 0; iter < num_iterations; iter++) {
      for (size_t i = 0; i < keys.size(); i++) {
        results[i] = query(keys[i]);
      }
    }
    auto end = std::chrono::high_resolution_clock::now();

    double total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                          end - start)
                          .count();
    double ns_per_query = total_ns / (keys.size() * num_iterations);
    return {results, ns_per_query};
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
    return _filter->getStats();
  }

  T _queryCore(const char *data, size_t length) const {
    if (_bucket_info.empty()) {
      if (_filter && _filter->getMostCommonValue()) {
        return *_filter->getMostCommonValue();
      }
      throw std::runtime_error("Cannot query empty CSF without filter");
    }

    __uint128_t signature = hashKey(data, length, _hash_store_seed);

    uint32_t bucket_id =
        getBucketID(signature, _num_buckets);

    const auto &info = _bucket_info[bucket_id];

    uint64_t e[3];
    signatureToEquation(signature, info.seed, info.num_variables, e);

    const uint64_t *arr = info.data;
    const int l = 64 - _max_codelength;
    auto getbits = [arr, l](uint32_t pos) __attribute__((always_inline)) {
      const uint64_t w = pos / 64;
      const int b = pos % 64;
      if (b <= l)
        return arr[w] << b >> l;
      return arr[w] << b >> l | arr[w + 1] >> (128 - (-l + 64) - b);
    };

    uint64_t encoded_value = getbits(e[0]) ^ getbits(e[1]) ^ getbits(e[2]);

    return _lookup_table.decode(encoded_value);
  }

  // Private constructor for cereal
  Csf() {}

  void _buildQueryCache() {
    _num_buckets = _solutions_and_seeds.size();
    if (!_code_length_counts.empty() && _max_codelength > 0) {
      _lookup_table = HuffmanLookupTable<T>(
          _code_length_counts, _ordered_symbols, _max_codelength);
    }
    _bucket_info.resize(_num_buckets);
    for (uint32_t i = 0; i < _num_buckets; i++) {
      auto &[solution, seed] = _solutions_and_seeds[i];
      _bucket_info[i] = {solution->backingArrayPtr(),
                         solution->numBits() - _max_codelength, seed};
    }
  }

  friend class cereal::access;
  template <class Archive> void save(Archive &archive) const {
    archive(_solutions_and_seeds, _code_length_counts, _ordered_symbols,
            _hash_store_seed, _filter, _max_codelength);
  }

  template <class Archive> void load(Archive &archive) {
    archive(_solutions_and_seeds, _code_length_counts, _ordered_symbols,
            _hash_store_seed, _filter, _max_codelength);
    _buildQueryCache();
  }

  std::vector<SubsystemSolutionSeedPair> _solutions_and_seeds;
  std::vector<uint32_t> _code_length_counts;
  std::vector<T> _ordered_symbols;
  uint32_t _hash_store_seed;
  PreFilterPtr<T> _filter = nullptr;
  uint32_t _max_codelength;

  // Query cache (built from _solutions_and_seeds, not serialized)
  uint32_t _num_buckets = 0;
  std::vector<BucketQueryInfo> _bucket_info;
  HuffmanLookupTable<T> _lookup_table;
};

} // namespace caramel