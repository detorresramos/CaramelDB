#pragma once

#include "src/construct/Csf.h"
#include "src/construct/multiset/MultisetCsf.h"

namespace caramel {

template <typename T> class RaggedMultisetCsf;
template <typename T>
using RaggedMultisetCsfPtr = std::shared_ptr<RaggedMultisetCsf<T>>;

// Ragged multiset: a length CSF plus one group per column (row presence
// differs per column, so columns cannot share a key set).
template <typename T> class RaggedMultisetCsf {
public:
  using Group = caramel::Group<T>;

  RaggedMultisetCsf(CsfPtr<uint32_t> length_csf, build::Index<T> columns)
      : _length_csf(std::move(length_csf)), _groups(std::move(columns.groups)),
        _shared_codebook(std::move(columns.shared_codebook)) {
    injectSharedCodebookAndBuildCaches();
  }

  std::vector<T> query(const std::string &key) const {
    return query(key.data(), key.size());
  }

  std::vector<T> query(const char *data, size_t length) const {
    uint32_t num_values = _length_csf->query(data, length);
    num_values = std::min(num_values, static_cast<uint32_t>(_groups.size()));
    const __uint128_t signature = hashKey(data, length);

    std::vector<T> outputs(num_values);
    for (size_t i = 0; i < num_values; i++) {
      const auto &group = _groups[i];
      const uint32_t bucket_id =
          group.num_buckets() ? getBucketID(signature, group.num_buckets()) : 0;
      outputs[i] = group.queryColumn(0, signature, bucket_id);
    }
    return outputs;
  }

  void save(const std::string &filename, uint32_t type_id = 0) const {
    saveWithTypeId(*this, filename, type_id);
  }

  static RaggedMultisetCsfPtr<T> load(const std::string &filename,
                                      uint32_t type_id = 0) {
    auto input = SafeFileIO::ifstream(filename, std::ios::binary);
    checkTypeId(input, type_id, filename);
    cereal::BinaryInputArchive archive(input);
    RaggedMultisetCsfPtr<T> csf(new RaggedMultisetCsf<T>());
    archive(*csf);
    return csf;
  }

private:
  RaggedMultisetCsf() = default;

  void injectSharedCodebookAndBuildCaches() {
    for (auto &g : _groups) {
      if (_shared_codebook) {
        g.injectSharedCodebook(_shared_codebook);
      }
      g.buildQueryCache();
    }
  }

  friend class cereal::access;
  template <class Archive> void save(Archive &archive) const {
    archive(_length_csf, _groups);
    // The shared codebook goes by value behind a presence flag rather than as
    // a shared_ptr, so cereal cannot alias it with a codebook of _length_csf.
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
