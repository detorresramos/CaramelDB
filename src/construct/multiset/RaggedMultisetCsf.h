#pragma once

#include "src/construct/Csf.h"
#include "src/construct/multiset/MultisetCsf.h"

namespace caramel {

template <typename T> class RaggedMultisetCsf;
template <typename T>
using RaggedMultisetCsfPtr = std::shared_ptr<RaggedMultisetCsf<T>>;

// Ragged multiset: one Group per column (key sets differ per column because
// of variable row lengths, so hash-store sharing across columns is not
// applicable). Still benefits from the flat arena layout per column.
template <typename T> class RaggedMultisetCsf {
public:
  using Group = typename MultisetCsf<T>::Group;

  RaggedMultisetCsf(CsfPtr<uint32_t> length_csf, std::vector<Group> groups)
      : _length_csf(std::move(length_csf)), _groups(std::move(groups)) {
    for (auto &g : _groups) {
      g.buildQueryCache();
    }
  }

  std::vector<T> query(const std::string &key) const {
    return query(key.data(), key.size());
  }

  std::vector<T> query(const char *data, size_t length) const {
    uint32_t num_values = _length_csf->query(data, length);
    num_values = std::min(num_values, static_cast<uint32_t>(_groups.size()));

    std::vector<T> outputs(num_values);

    for (size_t i = 0; i < num_values; i++) {
      const auto &group = _groups[i];
      const auto &col = group.columns[0];
      if (col.filter && !col.filter->contains(data, length)) {
        outputs[i] = *col.most_common_value;
      } else if (group.num_buckets == 0) {
        outputs[i] = *col.most_common_value;
      } else {
        __uint128_t signature = hashKey(data, length, group.hash_store_seed);
        uint32_t bucket_id = getBucketID(signature, group.num_buckets);
        const auto &info = group.bucket_col_info[bucket_id];

        uint64_t e[3];
        signatureToEquation(signature, info.seed, info.num_variables, e);

        const uint64_t *arr = info.data;
        const int l = 64 - static_cast<int>(col.max_codelength);
        auto getbits = [arr, l](uint32_t pos) __attribute__((always_inline)) {
          const uint64_t w = pos / 64;
          const int b = pos % 64;
          if (b <= l)
            return arr[w] << b >> l;
          return arr[w] << b >> l | arr[w + 1] >> (128 - (-l + 64) - b);
        };
        uint64_t encoded = getbits(e[0]) ^ getbits(e[1]) ^ getbits(e[2]);
        outputs[i] = canonicalDecodeFromNumber<T>(
            encoded, col.codebook->code_length_counts,
            col.codebook->ordered_symbols, col.max_codelength);
      }
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

  static RaggedMultisetCsfPtr<T> load(const std::string &filename,
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
    RaggedMultisetCsfPtr<T> deserialize_into(new RaggedMultisetCsf<T>());
    iarchive(*deserialize_into);
    return deserialize_into;
  }

private:
  RaggedMultisetCsf() {}

  friend class cereal::access;
  template <class Archive> void save(Archive &archive) const {
    archive(_length_csf, _groups);
  }

  template <class Archive> void load(Archive &archive) {
    archive(_length_csf, _groups);
    // Codebooks are serialized inside each group's columns (Ragged does not
    // use a shared codebook across the MultisetCsf); cereal's shared_ptr
    // tracking already dedupes per-column codebooks where applicable.
    for (auto &g : _groups) {
      g.buildQueryCache();
    }
  }

  CsfPtr<uint32_t> _length_csf;
  std::vector<Group> _groups;
};

} // namespace caramel
