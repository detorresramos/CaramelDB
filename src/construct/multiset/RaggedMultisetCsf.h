#pragma once

#include "src/construct/Csf.h"
#include "src/construct/multiset/MultisetCsf.h"

namespace caramel {

template <typename T> class RaggedMultisetCsf;
template <typename T>
using RaggedMultisetCsfPtr = std::shared_ptr<RaggedMultisetCsf<T>>;

template <typename T> class RaggedMultisetCsf {
public:
  using ColumnState = typename MultisetCsf<T>::ColumnState;

  RaggedMultisetCsf(CsfPtr<uint32_t> length_csf,
                    std::vector<ColumnState> columns)
      : _length_csf(std::move(length_csf)),
        _columns(std::move(columns)) {}

  std::vector<T> query(const std::string &key) const {
    return query(key.data(), key.size());
  }

  std::vector<T> query(const char *data, size_t length) const {
    uint32_t num_values = _length_csf->query(data, length);
    num_values = std::min(num_values, static_cast<uint32_t>(_columns.size()));

    std::vector<T> outputs(num_values);

    for (size_t i = 0; i < num_values; i++) {
      const auto &col = _columns[i];
      if (col.filter && !col.filter->contains(data, length)) {
        outputs[i] = *col.most_common_value;
      } else if (col.bucket_info.empty()) {
        outputs[i] = *col.most_common_value;
      } else {
        outputs[i] = queryCsfCore<T>(data, length, col.hash_store_seed,
                                     col.bucket_info, col.num_buckets,
                                     col.codebook->max_codelength,
                                     col.codebook->code_length_counts,
                                     col.codebook->ordered_symbols);
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
    archive(_length_csf, _columns);
  }

  template <class Archive> void load(Archive &archive) {
    archive(_length_csf, _columns);
    // buildQueryCache was moved out of ColumnState's cereal hook to support
    // the shared-codebook pattern (where codebook is injected externally).
    // Ragged serializes everything in one archive, so cereal's shared_ptr
    // tracking already dedupes the shared codebook — we just need to rebuild
    // query caches here.
    for (auto &col : _columns) {
      if (col.codebook) {
        col.buildQueryCache();
      }
    }
  }

  CsfPtr<uint32_t> _length_csf;
  std::vector<ColumnState> _columns;
};

} // namespace caramel
