#pragma once

#include "src/construct/CsfCodebook.h"
#include "src/construct/CsfQueryCore.h"
#include "src/construct/Csf.h"
#include <cereal/types/memory.hpp>
#include <cereal/types/optional.hpp>
#include <filesystem>

namespace caramel {

template <typename T> class MultisetCsf;
template <typename T> using MultisetCsfPtr = std::shared_ptr<MultisetCsf<T>>;

template <typename T> class MultisetCsf {
public:
  struct ColumnState {
    std::vector<SubsystemSolutionSeedPair> solutions_and_seeds;
    uint32_t hash_store_seed = 0;
    std::shared_ptr<CsfCodebook<T>> codebook;
    PreFilterPtr<T> filter;
    std::optional<T> most_common_value;
    // When true, the shared codebook lives in a sibling file (for
    // directory-based save) or in the enclosing object (for single-archive
    // save with cereal shared_ptr tracking), and must be injected before
    // buildQueryCache() is called.
    bool uses_shared_codebook = false;

    // Query caches (not serialized)
    std::vector<BucketQueryInfo> bucket_info;
    uint32_t num_buckets = 0;

    void buildQueryCache() {
      num_buckets = solutions_and_seeds.size();
      bucket_info.resize(num_buckets);
      for (uint32_t i = 0; i < num_buckets; i++) {
        auto &[solution, seed] = solutions_and_seeds[i];
        bucket_info[i] = {solution->backingArrayPtr(),
                          solution->numBits() - codebook->max_codelength, seed};
      }
    }

  private:
    friend class cereal::access;

    template <class Archive> void save(Archive &archive) const {
      archive(solutions_and_seeds, hash_store_seed, filter, most_common_value,
              uses_shared_codebook);
      if (!uses_shared_codebook) {
        archive(codebook);
      }
    }

    template <class Archive> void load(Archive &archive) {
      archive(solutions_and_seeds, hash_store_seed, filter, most_common_value,
              uses_shared_codebook);
      if (!uses_shared_codebook) {
        archive(codebook);
      }
      // Deferred: buildQueryCache() must be called by the enclosing load path
      // after the codebook is assigned (may be injected externally for the
      // shared-codebook case).
    }
  };

  MultisetCsf(std::vector<ColumnState> columns,
              std::shared_ptr<CsfCodebook<T>> shared_codebook = nullptr)
      : _columns(std::move(columns)),
        _shared_codebook(std::move(shared_codebook)) {}

  std::vector<T> query(const std::string &key, bool parallelize = true) const {
    return query(key.data(), key.size(), parallelize);
  }

  std::vector<T> query(const char *data, size_t length,
                       bool parallelize = true) const {
    std::vector<T> outputs(_columns.size());

#pragma omp parallel for default(none)                                         \
    shared(data, length, _columns, outputs) if (parallelize)
    for (size_t i = 0; i < _columns.size(); i++) {
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

  static void saveColumnState(const ColumnState &col, const std::string &path) {
    auto stream = SafeFileIO::ofstream(path, std::ios::binary);
    cereal::BinaryOutputArchive archive(stream);
    archive(col);
  }

  // Loads a column and runs buildQueryCache(). Only valid if the column does
  // not use a shared codebook (otherwise the codebook must be injected first).
  static ColumnState loadColumnState(const std::string &path) {
    auto stream = SafeFileIO::ifstream(path, std::ios::binary);
    cereal::BinaryInputArchive archive(stream);
    ColumnState col;
    archive(col);
    if (!col.uses_shared_codebook) {
      col.buildQueryCache();
    }
    return col;
  }

  static void saveSharedCodebook(const CsfCodebook<T> &cb,
                                 const std::string &path) {
    auto stream = SafeFileIO::ofstream(path, std::ios::binary);
    cereal::BinaryOutputArchive archive(stream);
    archive(cb);
  }

  static std::shared_ptr<CsfCodebook<T>>
  loadSharedCodebook(const std::string &path) {
    auto stream = SafeFileIO::ifstream(path, std::ios::binary);
    cereal::BinaryInputArchive archive(stream);
    auto cb = std::make_shared<CsfCodebook<T>>();
    archive(*cb);
    return cb;
  }

  void save(const std::string &path, const uint32_t type_id = 0) const {
    std::filesystem::create_directories(path);
    {
      auto meta = SafeFileIO::ofstream(path + "/metadata.bin", std::ios::binary);
      uint32_t num_cols = _columns.size();
      uint8_t uses_shared = _shared_codebook ? 1 : 0;
      meta.write(reinterpret_cast<const char *>(&type_id), sizeof(uint32_t));
      meta.write(reinterpret_cast<const char *>(&num_cols), sizeof(uint32_t));
      meta.write(reinterpret_cast<const char *>(&uses_shared), sizeof(uint8_t));
    }
    if (_shared_codebook) {
      saveSharedCodebook(*_shared_codebook, path + "/shared_codebook.bin");
    }
    for (size_t i = 0; i < _columns.size(); i++) {
      saveColumnState(_columns[i],
                      path + "/col_" + std::to_string(i) + ".bin");
    }
  }

  static MultisetCsfPtr<T> load(const std::string &path,
                                const uint32_t type_id = 0) {
    auto meta = SafeFileIO::ifstream(path + "/metadata.bin", std::ios::binary);
    uint32_t type_id_found = 0, num_cols = 0;
    uint8_t uses_shared = 0;
    meta.read(reinterpret_cast<char *>(&type_id_found), sizeof(uint32_t));
    if (type_id != type_id_found) {
      throw CsfDeserializationException(
          "Expected type_id to be " + std::to_string(type_id) +
          " but found type_id = " + std::to_string(type_id_found) +
          " when deserializing " + path);
    }
    meta.read(reinterpret_cast<char *>(&num_cols), sizeof(uint32_t));
    meta.read(reinterpret_cast<char *>(&uses_shared), sizeof(uint8_t));

    std::shared_ptr<CsfCodebook<T>> shared_cb;
    if (uses_shared) {
      shared_cb = loadSharedCodebook(path + "/shared_codebook.bin");
    }

    std::vector<ColumnState> columns(num_cols);
    for (uint32_t i = 0; i < num_cols; i++) {
      columns[i] =
          loadColumnState(path + "/col_" + std::to_string(i) + ".bin");
      if (columns[i].uses_shared_codebook) {
        columns[i].codebook = shared_cb;
        columns[i].buildQueryCache();
      }
    }
    return std::make_shared<MultisetCsf<T>>(std::move(columns), shared_cb);
  }

private:
  MultisetCsf() {}

  std::vector<ColumnState> _columns;
  std::shared_ptr<CsfCodebook<T>> _shared_codebook;
};

} // namespace caramel
