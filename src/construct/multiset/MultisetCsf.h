#pragma once

#include "src/construct/BucketedHashStore.h"
#include "src/construct/CsfCodebook.h"
#include "src/construct/CsfQueryCore.h"
#include "src/construct/Csf.h"
#include <cereal/types/memory.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/vector.hpp>
#include <filesystem>

namespace caramel {

template <typename T> class MultisetCsf;
template <typename T> using MultisetCsfPtr = std::shared_ptr<MultisetCsf<T>>;

// E3 (interleaved-bucket-layout): columns are grouped by shared active-key set
// (shared filter / no filter = one group, per-col filter = one group per col).
// Each group has a single hash store (seed + num_buckets) so the hot path
// computes signature/bucket once per group. Within a group, solution bits for
// all M columns of the same bucket are packed contiguously in a flat arena,
// making the per-column fan-out a sequential streaming read.
template <typename T> class MultisetCsf {
public:
  // Per-column metadata within a group. All bit-level data lives in the group
  // arena; this struct only describes how to interpret the arena for col c.
  struct GroupColumn {
    uint32_t output_index = 0;  // index into outputs[] returned by query()
    std::shared_ptr<CsfCodebook<T>> codebook;
    PreFilterPtr<T> filter;
    std::optional<T> most_common_value;
    // When true, the shared codebook is owned by the enclosing MultisetCsf and
    // must be injected during load before buildQueryCache().
    bool uses_shared_codebook = false;
    // max_codelength cached for hot path (mirrors codebook->max_codelength).
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

  // Flat bucket-contiguous arena:
  //   - solution_bits: concatenation of all per-(bucket, col) bit ranges.
  //   - bucket_offsets[b]: start bit position in solution_bits for bucket b's
  //     block. bucket_offsets has size num_buckets+1 (last = total bits).
  //   - per_col_bits[b*M + c]: length in bits of column c's sub-range in
  //     bucket b. Column c's bits live at:
  //       start = bucket_offsets[b] + sum_{c'<c} per_col_bits[b*M + c']
  //       end   = start + per_col_bits[b*M + c]
  //   - per_col_seeds[b*M + c]: subsystem solve seed for (b, c).
  // Layout guarantees: for a single bucket b, columns 0..M-1 are stored
  // contiguously, so the inner query loop streams through the bucket block.
  struct BucketArena {
    std::vector<uint64_t> solution_bits;
    std::vector<uint64_t> bucket_offsets;
    std::vector<uint32_t> per_col_bits;
    std::vector<uint32_t> per_col_seeds;
    uint32_t num_cols = 0;
    uint32_t num_buckets = 0;

   private:
    friend class cereal::access;
    template <class Archive> void serialize(Archive &archive) {
      archive(solution_bits, bucket_offsets, per_col_bits, per_col_seeds,
              num_cols, num_buckets);
    }
  };

  // A group of columns that share a single hash store and arena. Columns end
  // up in the same group iff their active-key sets are identical (shared
  // filter, or no filter at all).
  struct Group {
    uint32_t hash_store_seed = 0;
    uint32_t num_buckets = 0;
    BucketArena arena;
    std::vector<GroupColumn> columns;

    // Query caches: flat array of BucketQueryInfo laid out as
    // [bucket 0 col 0, bucket 0 col 1, ..., bucket 0 col M-1,
    //  bucket 1 col 0, ...]. Same interleaved layout as the arena itself.
    // Not serialized.
    std::vector<BucketQueryInfo> bucket_col_info;

    void buildQueryCache() {
      const uint32_t M = arena.num_cols;
      const uint32_t B = arena.num_buckets;
      bucket_col_info.assign(static_cast<size_t>(M) * B, BucketQueryInfo{});

      const uint64_t *backing = arena.solution_bits.data();
      // For each (bucket, col), the effective "data pointer" is the backing
      // array offset to the start of that range, rounded down to 64-bit word.
      // But we need bit-level indexing; we store word-aligned arr + a bit
      // offset. To keep BucketQueryInfo unchanged, we wrap using a helper:
      // for (b, c), the solution bits start at absolute bit position
      //   start_bit = bucket_offsets[b] + prefix_sum(per_col_bits[b*M..b*M+c])
      // num_variables = per_col_bits[b*M + c] - col_max_codelength.
      //
      // queryCsfCore wants a uint64_t* that lets it read bits with an
      // absolute bit offset equal to the range start. The existing code uses
      // `arr[word]` with `pos / 64` and `pos % 64`. We pass a pointer to the
      // word containing the start, but only if the range starts at a word
      // boundary. To avoid adding bit-shifting fixup to the hot path, we
      // require start_bit % 64 == 0 (enforced by padding during build).
      for (uint32_t b = 0; b < B; b++) {
        uint64_t bit_off = arena.bucket_offsets[b];
        for (uint32_t c = 0; c < M; c++) {
          uint32_t len_bits = arena.per_col_bits[b * M + c];
          uint32_t seed = arena.per_col_seeds[b * M + c];
          uint32_t max_cl = columns[c].max_codelength;
          uint64_t word = bit_off / 64;
          bucket_col_info[b * M + c] = BucketQueryInfo{
              backing + word,
              static_cast<uint32_t>(len_bits - max_cl),
              seed};
          // Advance to the next word-aligned boundary — each (bucket, col)
          // range is padded up to 64 bits so query reads are word-indexed.
          bit_off += (len_bits + 63u) & ~static_cast<uint64_t>(63u);
        }
      }
    }

   private:
    friend class cereal::access;
    template <class Archive> void save(Archive &archive) const {
      archive(hash_store_seed, num_buckets, arena, columns);
    }
    template <class Archive> void load(Archive &archive) {
      archive(hash_store_seed, num_buckets, arena, columns);
    }
  };

  MultisetCsf(std::vector<Group> groups, uint32_t total_cols,
              std::shared_ptr<CsfCodebook<T>> shared_codebook = nullptr)
      : _groups(std::move(groups)), _total_cols(total_cols),
        _shared_codebook(std::move(shared_codebook)) {
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

  std::vector<T> query(const std::string &key, bool parallelize = true) const {
    return query(key.data(), key.size(), parallelize);
  }

  std::vector<T> query(const char *data, size_t length,
                       bool parallelize = true) const {
    std::vector<T> outputs(_total_cols);

#pragma omp parallel for default(none)                                         \
    shared(data, length, _groups, outputs) if (parallelize)
    for (size_t gi = 0; gi < _groups.size(); gi++) {
      const auto &group = _groups[gi];
      // Group-level hash: compute signature + bucket_id once for the whole
      // group. This is the E2/E3 payoff — per-column query no longer rehashes.
      __uint128_t signature = hashKey(data, length, group.hash_store_seed);
      uint32_t bucket_id = group.num_buckets
                               ? getBucketID(signature, group.num_buckets)
                               : 0;
      const uint32_t M = group.arena.num_cols;

      for (size_t ci = 0; ci < group.columns.size(); ci++) {
        const auto &col = group.columns[ci];
        T value;
        if (col.filter && !col.filter->contains(data, length)) {
          value = *col.most_common_value;
        } else if (group.num_buckets == 0) {
          value = *col.most_common_value;
        } else {
          const auto &info = group.bucket_col_info[bucket_id * M + ci];
          uint64_t e[3];
          signatureToEquation(signature, info.seed, info.num_variables, e);

          const uint64_t *arr = info.data;
          const int l = 64 - static_cast<int>(col.max_codelength);
          auto getbits = [arr, l](uint32_t pos)
                             __attribute__((always_inline)) {
            const uint64_t w = pos / 64;
            const int b = pos % 64;
            if (b <= l)
              return arr[w] << b >> l;
            return arr[w] << b >> l | arr[w + 1] >> (128 - (-l + 64) - b);
          };
          uint64_t encoded =
              getbits(e[0]) ^ getbits(e[1]) ^ getbits(e[2]);
          value = canonicalDecodeFromNumber<T>(
              encoded, col.codebook->code_length_counts,
              col.codebook->ordered_symbols, col.max_codelength);
        }
        outputs[col.output_index] = value;
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
