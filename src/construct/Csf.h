#pragma once

#include "CsfStats.h"
#include "Group.h"
#include <cereal/archives/binary.hpp>
#include <limits>
#include <memory>
#include <src/build/Index.h>
#include <src/utils/SafeFileIO.h>

namespace caramel {

template <typename T> class Csf;
template <typename T> using CsfPtr = std::shared_ptr<Csf<T>>;

class CsfDeserializationException : public std::runtime_error {
public:
  explicit CsfDeserializationException(const std::string &message)
      : std::runtime_error("Cannot deserialize CSF: " + message) {};
};

// Checks the type_id header written by saveWithTypeId.
inline void checkTypeId(std::istream &input, uint32_t type_id,
                        const std::string &filename) {
  uint32_t found = 0;
  input.read(reinterpret_cast<char *>(&found), sizeof(uint32_t));
  if (found != type_id) {
    throw CsfDeserializationException(
        "Expected type_id to be " + std::to_string(type_id) +
        " but found type_id = " + std::to_string(found) +
        " when deserializing " + filename);
  }
}

template <typename Obj>
void saveWithTypeId(const Obj &obj, const std::string &filename,
                    uint32_t type_id) {
  auto output = SafeFileIO::ofstream(filename, std::ios::binary);
  output.write(reinterpret_cast<const char *>(&type_id), sizeof(uint32_t));
  cereal::BinaryOutputArchive archive(output);
  archive(obj);
}

// A single-column CSF: one group with one column.
template <typename T> class Csf {
public:
  explicit Csf(build::Index<T> index) : _group(std::move(index.groups.at(0))) {
    _group.buildQueryCache();
  }

  T query(const std::string &key) const { return query(key.data(), key.size()); }

  T query(const char *data, size_t length) const {
    return queryHashed(hashKey(data, length));
  }

  T queryHashed(const __uint128_t &signature) const {
    const auto &col = _group.columns[0];
    if (col.filter && !col.filter->containsHash(filterHash(signature))) {
      return *col.most_common_value;
    }
    if (_group.arena.num_buckets == 0) {
      if (col.most_common_value) {
        return *col.most_common_value;
      }
      throw std::runtime_error("Cannot query empty CSF without filter");
    }
    const uint32_t bucket = getBucketID(signature, _group.arena.num_buckets);
    return decodeBucketColumn<T>(RibbonHash(signature), _group.cell(bucket, 0),
                                 col.codebook->code_length_counts,
                                 col.codebook->ordered_symbols);
  }

  void save(const std::string &filename, uint32_t type_id = 0) const {
    saveWithTypeId(*this, filename, type_id);
  }

  static CsfPtr<T> load(const std::string &filename, uint32_t type_id = 0) {
    auto input = SafeFileIO::ifstream(filename, std::ios::binary);
    checkTypeId(input, type_id, filename);
    cereal::BinaryInputArchive archive(input);
    CsfPtr<T> csf(new Csf<T>());
    archive(*csf);
    return csf;
  }

  PreFilterPtr<T> getFilter() const { return _group.columns[0].filter; }

  CsfStats getStats() const {
    const auto &arena = _group.arena;
    const auto &col = _group.columns[0];
    CsfStats stats;

    auto &bs = stats.bucket_stats;
    bs.num_buckets = arena.num_buckets;
    bs.total_solution_bits = 0;
    bs.min_solution_bits = arena.num_buckets ? std::numeric_limits<size_t>::max() : 0;
    bs.max_solution_bits = 0;
    for (uint32_t vars : arena.per_col_vars) {
      size_t bits = vars + RIBBON_WIDTH - 1;
      bs.total_solution_bits += bits;
      bs.min_solution_bits = std::min(bs.min_solution_bits, bits);
      bs.max_solution_bits = std::max(bs.max_solution_bits, bits);
    }
    bs.avg_solution_bits =
        bs.num_buckets ? static_cast<double>(bs.total_solution_bits) / bs.num_buckets
                       : 0;

    auto &hs = stats.huffman_stats;
    hs.num_unique_symbols = col.codebook ? col.codebook->ordered_symbols.size() : 0;
    hs.max_code_length = col.max_codelength;
    hs.code_length_distribution =
        col.codebook ? col.codebook->code_length_counts : std::vector<uint32_t>{};
    size_t total_symbols = 0, total_bits = 0;
    for (size_t len = 1; len < hs.code_length_distribution.size(); len++) {
      total_symbols += hs.code_length_distribution[len];
      total_bits += hs.code_length_distribution[len] * len;
    }
    hs.avg_bits_per_symbol =
        total_symbols ? static_cast<double>(total_bits) / total_symbols : 0.0;

    stats.solution_bytes = static_cast<double>(arena.solution_bits.size() * 8);

    stats.filter_bytes = 0;
    if (col.filter) {
      stats.filter_stats = col.filter->getStats();
      if (stats.filter_stats) {
        stats.filter_bytes = static_cast<double>(stats.filter_stats->size_bytes);
      }
    }

    size_t metadata = hs.code_length_distribution.size() * sizeof(uint32_t) +
                      hs.num_unique_symbols * sizeof(T) + sizeof(uint32_t);
    // Per bucket: band starts and seed.
    metadata += arena.num_buckets * (sizeof(uint32_t) + sizeof(uint8_t));
    stats.metadata_bytes = static_cast<double>(metadata);

    stats.in_memory_bytes = static_cast<size_t>(
        stats.solution_bytes + stats.filter_bytes + stats.metadata_bytes);
    return stats;
  }

private:
  Csf() = default;

  friend class cereal::access;
  template <class Archive> void save(Archive &archive) const { archive(_group); }
  template <class Archive> void load(Archive &archive) {
    archive(_group);
    _group.buildQueryCache();
  }

  Group<T> _group;
};

} // namespace caramel
