#pragma once

#include "src/construct/Csf.h"
#include "src/construct/Group.h"

namespace caramel {

template <typename T> class PackedCsf;
template <typename T> using PackedCsfPtr = std::shared_ptr<PackedCsf<T>>;

// One code stream per key: the row's Huffman codes followed by STOP, solved
// as one wide value. A single group with a single column.
template <typename T> class PackedCsf {
public:
  explicit PackedCsf(build::Index<T> index)
      : _group(std::move(index.groups.at(0))), _max_pad(index.max_pad),
        _stop_symbol(index.stop_symbol) {
    _group.buildQueryCache();
  }

  std::vector<T> query(const std::string &key) const {
    return query(key.data(), key.size());
  }

  std::vector<T> query(const char *data, size_t length) const {
    const __uint128_t signature = hashKey(data, length);
    const auto &info =
        _group.cell(getBucketID(signature, _group.num_buckets()), 0);
    const auto &codebook = *_group.columns[0].codebook;

    uint64_t e[3];
    signatureToEquation(signature, info.seed, info.num_variables, e);

    const uint64_t *arr = info.data;
    const uint32_t max_cl = codebook.max_codelength;
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
    while (bit_pos + max_cl <= _max_pad) {
      uint64_t encoded = getbits(e[0] + bit_pos) ^ getbits(e[1] + bit_pos) ^
                         getbits(e[2] + bit_pos);
      auto [symbol, code_len] = decodeWithLength(encoded, codebook);
      if (symbol == _stop_symbol) {
        break;
      }
      result.push_back(symbol);
      bit_pos += code_len;
    }
    return result;
  }

  void save(const std::string &filename, uint32_t type_id = 0) const {
    saveWithTypeId(*this, filename, type_id);
  }

  static PackedCsfPtr<T> load(const std::string &filename, uint32_t type_id = 0) {
    auto input = SafeFileIO::ifstream(filename, std::ios::binary);
    checkTypeId(input, type_id, filename);
    cereal::BinaryInputArchive archive(input);
    PackedCsfPtr<T> csf(new PackedCsf<T>());
    archive(*csf);
    return csf;
  }

private:
  static std::pair<T, uint32_t> decodeWithLength(uint64_t encoded_value,
                                                 const CsfCodebook<T> &codebook) {
    const auto &clc = codebook.code_length_counts;
    const auto &symbols = codebook.ordered_symbols;
    const uint32_t max_cl = codebook.max_codelength;
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

  PackedCsf() = default;

  friend class cereal::access;
  template <class Archive> void save(Archive &archive) const {
    archive(_group, _max_pad, _stop_symbol);
  }
  template <class Archive> void load(Archive &archive) {
    archive(_group, _max_pad, _stop_symbol);
    _group.buildQueryCache();
  }

  Group<T> _group;
  uint32_t _max_pad = 0;
  T _stop_symbol{};
};

} // namespace caramel
