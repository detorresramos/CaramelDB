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

  RaggedMultisetCsf(CsfPtr<uint32_t> length_csf, std::vector<Group> groups,
                    std::shared_ptr<CsfCodebook<T>> shared_codebook = nullptr)
      : _length_csf(std::move(length_csf)), _groups(std::move(groups)),
        _shared_codebook(std::move(shared_codebook)) {
    injectSharedCodebookAndBuildCaches();
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
      __uint128_t signature = hashKey(data, length, group.hash_store_seed);
      uint32_t bucket_id = group.num_buckets()
                               ? getBucketID(signature, group.num_buckets())
                               : 0;
      outputs[i] = group.queryColumn(0, data, length, signature, bucket_id);
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

  // Inject the shared codebook into the columns that use it (their codebook
  // ptr is null after load), then build the per-group query caches.
  void injectSharedCodebookAndBuildCaches() {
    for (auto &g : _groups) {
      if (_shared_codebook) {
        for (auto &col : g.columns) {
          if (col.uses_shared_codebook) {
            col.codebook = _shared_codebook;
          }
        }
      }
      g.buildQueryCache();
    }
  }

  friend class cereal::access;
  template <class Archive> void save(Archive &archive) const {
    archive(_length_csf, _groups);
    // Serialize the shared codebook by value behind a presence flag, rather
    // than as a shared_ptr, to avoid cereal aliasing it with the same-typed
    // codebook inside _length_csf.
    bool has_shared = _shared_codebook != nullptr;
    archive(has_shared);
    if (has_shared) {
      archive(*_shared_codebook);
    }
  }

  template <class Archive> void load(Archive &archive) {
    archive(_length_csf, _groups);
    bool has_shared = false;
    archive(has_shared);
    if (has_shared) {
      _shared_codebook = std::make_shared<CsfCodebook<T>>();
      archive(*_shared_codebook);
    }
    injectSharedCodebookAndBuildCaches();
  }

  CsfPtr<uint32_t> _length_csf;
  std::vector<Group> _groups;
  std::shared_ptr<CsfCodebook<T>> _shared_codebook;
};

} // namespace caramel
