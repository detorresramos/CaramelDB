#pragma once

#include "src/construct/CsfCodebook.h"
#include "src/construct/CsfQueryCore.h"
#include "src/construct/Csf.h"
#include <cereal/types/memory.hpp>
#include <cereal/types/optional.hpp>

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
      archive(solutions_and_seeds, hash_store_seed, codebook, filter,
              most_common_value);
    }

    template <class Archive> void load(Archive &archive) {
      archive(solutions_and_seeds, hash_store_seed, codebook, filter,
              most_common_value);
      buildQueryCache();
    }
  };

  MultisetCsf(std::vector<ColumnState> columns)
      : _columns(std::move(columns)) {}

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
                                     col.codebook->lookup_table);
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

  static MultisetCsfPtr<T> load(const std::string &filename,
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
    MultisetCsfPtr<T> deserialize_into(new MultisetCsf<T>());
    iarchive(*deserialize_into);
    return deserialize_into;
  }

private:
  MultisetCsf() {}

  friend class cereal::access;
  template <class Archive> void serialize(Archive &archive) {
    archive(_columns);
  }

  std::vector<ColumnState> _columns;
};

} // namespace caramel
