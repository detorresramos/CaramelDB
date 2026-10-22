#pragma once

#include "CsfCodebook.h"
#include "CsfQueryCore.h"
#include "Hashing.h"
#include <cereal/types/memory.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/vector.hpp>
#include <src/construct/filter/PreFilter.h>

namespace caramel {

// Solved bits of one group: B buckets x M columns of cells, bucket-major (all
// M columns of bucket 0, then bucket 1, ...) so a query streams one bucket's
// columns contiguously. Cell (b, c) lives at index b * M + c; each cell's range
// starts on a 64-bit word boundary.
struct BucketArena {
  std::vector<uint64_t> solution_bits; // + 1 trailing guard word
  std::vector<uint32_t> per_col_vars;  // variables of each cell's system
  std::vector<uint32_t> per_col_seeds; // solve seed of each cell
  std::vector<uint64_t> per_col_word_off;
  uint32_t num_cols = 0;
  uint32_t num_buckets = 0;

private:
  friend class cereal::access;
  template <class Archive> void serialize(Archive &archive) {
    archive(solution_bits, per_col_vars, per_col_seeds, per_col_word_off,
            num_cols, num_buckets);
  }
};

// How to decode one column of a group.
template <typename T> struct GroupColumn {
  uint32_t output_index = 0;
  std::shared_ptr<CsfCodebook<T>> codebook;
  PreFilterPtr<T> filter;
  std::optional<T> most_common_value;
  // The shared codebook is owned (and serialized once) by the enclosing index,
  // which re-injects it after load.
  bool uses_shared_codebook = false;
  uint32_t max_codelength = 0;

private:
  friend class cereal::access;
  template <class Archive> void save(Archive &archive) const {
    archive(output_index, filter, most_common_value, uses_shared_codebook,
            max_codelength);
    if (!uses_shared_codebook) {
      archive(codebook);
    }
  }
  template <class Archive> void load(Archive &archive) {
    archive(output_index, filter, most_common_value, uses_shared_codebook,
            max_codelength);
    if (!uses_shared_codebook) {
      archive(codebook);
    }
  }
};

// Columns that share one key set (same filter, same row presence) and one
// bucket count, so a query computes one bucket id for all of them.
template <typename T> struct Group {
  BucketArena arena;
  std::vector<GroupColumn<T>> columns;

  // Not serialized: [bucket 0 col 0, bucket 0 col 1, ..., bucket 1 col 0, ...].
  std::vector<BucketQueryInfo> bucket_col_info;

  uint32_t num_buckets() const { return arena.num_buckets; }

  void buildQueryCache() {
    const size_t cells = static_cast<size_t>(arena.num_cols) * arena.num_buckets;
    bucket_col_info.resize(cells);
    const uint64_t *backing = arena.solution_bits.data();
    for (size_t idx = 0; idx < cells; idx++) {
      bucket_col_info[idx] = BucketQueryInfo{
          backing + arena.per_col_word_off[idx], arena.per_col_vars[idx],
          arena.per_col_seeds[idx]};
    }
  }

  void injectSharedCodebook(const std::shared_ptr<CsfCodebook<T>> &shared) {
    for (auto &col : columns) {
      if (col.uses_shared_codebook) {
        col.codebook = shared;
      }
    }
  }

  const BucketQueryInfo &cell(uint32_t bucket_id, size_t ci) const {
    return bucket_col_info[static_cast<size_t>(bucket_id) * arena.num_cols + ci];
  }

  T queryColumn(size_t ci, const __uint128_t &signature,
                uint32_t bucket_id) const {
    const auto &col = columns[ci];
    if (col.filter && !col.filter->containsHash(filterHash(signature))) {
      return col.most_common_value.value_or(T{});
    }
    if (arena.num_buckets == 0 || !col.codebook) {
      return col.most_common_value.value_or(T{});
    }
    return decodeBucketColumn<T>(signature, cell(bucket_id, ci),
                                 col.max_codelength,
                                 col.codebook->code_length_counts,
                                 col.codebook->ordered_symbols);
  }

private:
  friend class cereal::access;
  template <class Archive> void serialize(Archive &archive) {
    archive(arena, columns);
  }
};

} // namespace caramel
