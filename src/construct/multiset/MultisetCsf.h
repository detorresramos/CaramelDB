#pragma once

#include "src/construct/Csf.h"
#include "src/construct/Group.h"
#include <cereal/types/memory.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/vector.hpp>
#include <filesystem>

namespace caramel {

template <typename T> class MultisetCsf;
template <typename T> using MultisetCsfPtr = std::shared_ptr<MultisetCsf<T>>;

// Columns are grouped by shared key set (shared filter / no filter = one
// group, per-column filter = one group per column), then by bucket count. All
// groups hash with the same seed, so a query computes one signature.
template <typename T> class MultisetCsf {
public:
  using Group = caramel::Group<T>;

  explicit MultisetCsf(build::Index<T> index)
      : MultisetCsf(std::move(index.groups), index.num_cols,
                    std::move(index.shared_codebook)) {}

  MultisetCsf(std::vector<Group> groups, uint32_t total_cols,
              std::shared_ptr<CsfCodebook<T>> shared_codebook = nullptr)
      : _groups(std::move(groups)), _total_cols(total_cols),
        _shared_codebook(std::move(shared_codebook)) {
    for (auto &g : _groups) {
      if (_shared_codebook) {
        g.injectSharedCodebook(_shared_codebook);
      }
      g.buildQueryCache();
    }
  }

  std::vector<T> query(const std::string &key, bool parallelize = true) const {
    return query(key.data(), key.size(), parallelize);
  }

  std::vector<T> query(const char *data, size_t length,
                       bool parallelize = true) const {
    std::vector<T> outputs(_total_cols);
    const __uint128_t signature = hashKey(data, length);

#pragma omp parallel for default(none)                                         \
    shared(signature, _groups, outputs) if (parallelize)
    for (size_t gi = 0; gi < _groups.size(); gi++) {
      const auto &group = _groups[gi];
      const uint32_t bucket_id =
          group.num_buckets() ? getBucketID(signature, group.num_buckets()) : 0;
      for (size_t ci = 0; ci < group.columns.size(); ci++) {
        outputs[group.columns[ci].output_index] =
            group.queryColumn(ci, signature, bucket_id);
      }
    }
    return outputs;
  }

  void save(const std::string &path, const uint32_t type_id = 0) const {
    std::filesystem::create_directories(path);
    {
      auto meta = SafeFileIO::ofstream(path + "/metadata.bin", std::ios::binary);
      uint32_t num_groups = _groups.size();
      uint8_t uses_shared = _shared_codebook ? 1 : 0;
      meta.write(reinterpret_cast<const char *>(&type_id), sizeof(uint32_t));
      meta.write(reinterpret_cast<const char *>(&_total_cols), sizeof(uint32_t));
      meta.write(reinterpret_cast<const char *>(&num_groups), sizeof(uint32_t));
      meta.write(reinterpret_cast<const char *>(&uses_shared), sizeof(uint8_t));
    }
    if (_shared_codebook) {
      auto stream =
          SafeFileIO::ofstream(path + "/shared_codebook.bin", std::ios::binary);
      cereal::BinaryOutputArchive archive(stream);
      archive(*_shared_codebook);
    }
    for (size_t i = 0; i < _groups.size(); i++) {
      auto stream = SafeFileIO::ofstream(
          path + "/group_" + std::to_string(i) + ".bin", std::ios::binary);
      cereal::BinaryOutputArchive archive(stream);
      archive(_groups[i]);
    }
  }

  static MultisetCsfPtr<T> load(const std::string &path,
                                const uint32_t type_id = 0) {
    auto meta = SafeFileIO::ifstream(path + "/metadata.bin", std::ios::binary);
    uint32_t type_id_found = 0, total_cols = 0, num_groups = 0;
    uint8_t uses_shared = 0;
    meta.read(reinterpret_cast<char *>(&type_id_found), sizeof(uint32_t));
    if (type_id != type_id_found) {
      throw CsfDeserializationException(
          "Expected type_id to be " + std::to_string(type_id) +
          " but found type_id = " + std::to_string(type_id_found) +
          " when deserializing " + path);
    }
    meta.read(reinterpret_cast<char *>(&total_cols), sizeof(uint32_t));
    meta.read(reinterpret_cast<char *>(&num_groups), sizeof(uint32_t));
    meta.read(reinterpret_cast<char *>(&uses_shared), sizeof(uint8_t));

    std::shared_ptr<CsfCodebook<T>> shared_cb;
    if (uses_shared) {
      auto stream =
          SafeFileIO::ifstream(path + "/shared_codebook.bin", std::ios::binary);
      cereal::BinaryInputArchive archive(stream);
      shared_cb = std::make_shared<CsfCodebook<T>>();
      archive(*shared_cb);
    }

    std::vector<Group> groups(num_groups);
    for (uint32_t i = 0; i < num_groups; i++) {
      auto stream = SafeFileIO::ifstream(
          path + "/group_" + std::to_string(i) + ".bin", std::ios::binary);
      cereal::BinaryInputArchive archive(stream);
      archive(groups[i]);
    }

    return std::make_shared<MultisetCsf<T>>(std::move(groups), total_cols,
                                             shared_cb);
  }

  // Accessors (used by RaggedMultisetCsf and tests)
  const std::vector<Group> &groups() const { return _groups; }
  uint32_t totalCols() const { return _total_cols; }
  std::shared_ptr<CsfCodebook<T>> sharedCodebook() const {
    return _shared_codebook;
  }

private:
  MultisetCsf() {}

  std::vector<Group> _groups;
  uint32_t _total_cols = 0;
  std::shared_ptr<CsfCodebook<T>> _shared_codebook;
};

} // namespace caramel
