#pragma once

#include "src/construct/CsfCodebook.h"
#include "src/construct/CsfQueryCore.h"
#include "src/construct/Csf.h"
#include <cereal/types/memory.hpp>
#include <cereal/types/optional.hpp>

namespace caramel {

template <typename T> class PackedCsf;
template <typename T> using PackedCsfPtr = std::shared_ptr<PackedCsf<T>>;

template <typename T> class PackedCsf {
public:
  PackedCsf(std::vector<SubsystemSolutionSeedPair> solutions_and_seeds,
            uint32_t hash_store_seed,
            std::shared_ptr<CsfCodebook<T>> codebook,
            uint32_t max_total_bits, T stop_symbol)
      : _solutions_and_seeds(std::move(solutions_and_seeds)),
        _hash_store_seed(hash_store_seed), _codebook(std::move(codebook)),
        _max_total_bits(max_total_bits), _stop_symbol(stop_symbol) {
    _buildQueryCache();
  }

  std::vector<T> query(const std::string &key) const {
    return query(key.data(), key.size());
  }

  std::vector<T> query(const char *data, size_t length) const {
    __uint128_t signature = hashKey(data, length, _hash_store_seed);
    uint32_t bucket_id = getBucketID(signature, _num_buckets);
    const auto &info = _bucket_info[bucket_id];

    uint64_t e[3];
    signatureToEquation(signature, info.seed, info.num_variables, e);

    const uint64_t *arr = info.data;
    const uint32_t max_cl = _codebook->max_codelength;
    const int l = 64 - max_cl;

    auto getbits = [arr, l](uint32_t pos) __attribute__((always_inline)) {
      const uint64_t w = pos / 64;
      const int b = pos % 64;
      if (b <= l)
        return arr[w] << b >> l;
      return arr[w] << b >> l | arr[w + 1] >> (128 - (-l + 64) - b);
    };

    std::vector<T> result;
    uint32_t bit_pos = 0;

    while (bit_pos + max_cl <= _max_total_bits) {
      uint64_t encoded =
          getbits(e[0] + bit_pos) ^ getbits(e[1] + bit_pos) ^
          getbits(e[2] + bit_pos);

      auto [symbol, code_len] = _decodeWithLength(encoded);

      if (symbol == _stop_symbol) {
        break;
      }
      result.push_back(symbol);
      bit_pos += code_len;
    }

    return result;
  }

  void save(const std::string &filename, const uint32_t type_id = 0) const {
    auto output_stream = SafeFileIO::ofstream(filename, std::ios::binary);
    output_stream.write(reinterpret_cast<const char *>(&type_id),
                        sizeof(uint32_t));
    cereal::BinaryOutputArchive oarchive(output_stream);
    oarchive(*this);
  }

  static PackedCsfPtr<T> load(const std::string &filename,
                              const uint32_t type_id = 0) {
    auto input_stream = SafeFileIO::ifstream(filename, std::ios::binary);
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
    PackedCsfPtr<T> deserialize_into(new PackedCsf<T>());
    iarchive(*deserialize_into);
    return deserialize_into;
  }

private:
  std::pair<T, uint32_t>
  _decodeWithLength(uint64_t encoded_value) const {
    const auto &clc = _codebook->code_length_counts;
    const auto &symbols = _codebook->ordered_symbols;
    uint32_t max_cl = _codebook->max_codelength;

    int code = 0;
    int first = 0;
    int index = 0;
    for (uint32_t i = 1; i < clc.size(); i++) {
      uint32_t next_bit = (encoded_value >> (max_cl - i)) & 1ULL;
      code = code | next_bit;
      int count = clc[i];
      if (code - count < first) {
        return {symbols[index + (code - first)], i};
      }
      index += count;
      first += count;
      first <<= 1;
      code <<= 1;
    }
    throw std::invalid_argument("Invalid code in packed CSF decode");
  }

  PackedCsf() {}

  void _buildQueryCache() {
    _num_buckets = _solutions_and_seeds.size();
    _bucket_info.resize(_num_buckets);
    for (uint32_t i = 0; i < _num_buckets; i++) {
      auto &[solution, seed] = _solutions_and_seeds[i];
      _bucket_info[i] = {solution->backingArrayPtr(),
                         solution->numBits() - _max_total_bits, seed};
    }
  }

  friend class cereal::access;

  template <class Archive> void save(Archive &archive) const {
    archive(_solutions_and_seeds, _hash_store_seed, _codebook, _max_total_bits,
            _stop_symbol);
  }

  template <class Archive> void load(Archive &archive) {
    archive(_solutions_and_seeds, _hash_store_seed, _codebook, _max_total_bits,
            _stop_symbol);
    _buildQueryCache();
  }

  std::vector<SubsystemSolutionSeedPair> _solutions_and_seeds;
  uint32_t _hash_store_seed = 0;
  std::shared_ptr<CsfCodebook<T>> _codebook;
  uint32_t _max_total_bits = 0;
  T _stop_symbol{};

  // Query cache
  uint32_t _num_buckets = 0;
  std::vector<BucketQueryInfo> _bucket_info;
};

} // namespace caramel
