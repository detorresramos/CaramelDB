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
      : _group(std::move(index.groups.at(0))), _max_row_bits(index.max_row_bits),
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

    RibbonReader bits(info.data, RibbonHash(signature), info.seed,
                      info.num_variables);
    std::vector<T> result;
    // A key outside the build reads noise; the longest row bounds the decode.
    uint32_t bit_pos = 0;
    while (bit_pos < _max_row_bits) {
      const T symbol = canonicalDecodeNext<T>(
          bits, codebook.code_length_counts, codebook.ordered_symbols, &bit_pos);
      if (symbol == _stop_symbol) {
        break;
      }
      result.push_back(symbol);
    }
    return result;
  }

  void save(const std::string &filename, uint32_t type_id = 0) const {
    saveWithTypeId(*this, filename, type_id);
  }

  static PackedCsfPtr<T> load(const std::string &filename, uint32_t type_id = 0) {
    auto input = SafeFileIO::ifstream(filename, std::ios::binary);
    checkHeader(input, type_id, filename);
    cereal::BinaryInputArchive archive(input);
    PackedCsfPtr<T> csf(new PackedCsf<T>());
    archive(*csf);
    return csf;
  }

private:
  PackedCsf() = default;

  friend class cereal::access;
  template <class Archive> void save(Archive &archive) const {
    archive(_group, _max_row_bits, _stop_symbol);
  }
  template <class Archive> void load(Archive &archive) {
    archive(_group, _max_row_bits, _stop_symbol);
    _group.buildQueryCache();
  }

  Group<T> _group;
  uint32_t _max_row_bits = 0;
  T _stop_symbol{};
};

} // namespace caramel
