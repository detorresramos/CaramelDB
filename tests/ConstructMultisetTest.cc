#include <gtest/gtest.h>
#include <cstdint>
#include <string>
#include <vector>
#include <src/construct/multiset/ConstructMultiset.h>
#include <src/construct/multiset/MultisetConfig.h>

namespace caramel::tests {

namespace {

std::vector<std::string> makeKeys(size_t n) {
  std::vector<std::string> keys;
  keys.reserve(n);
  for (size_t i = 0; i < n; i++) {
    keys.push_back("key_" + std::to_string(i));
  }
  return keys;
}

// Asserts the structural invariants fillArena() promises for one group's arena,
// independent of any query path:
//   - ranges are packed bucket-major, contiguous, in whole words.
//   - per_col_word_off matches the documented offset walk exactly.
//   - solution_bits has a trailing guard word so the decode's arr[w+1] read is
//     always in-bounds.
template <typename T>
void checkArenaInvariants(const typename MultisetCsf<T>::Group &group) {
  const auto &arena = group.arena;
  const uint32_t M = arena.num_cols;
  const uint32_t B = arena.num_buckets;

  if (B == 0) {
    return;  // degenerate group: no arena content to validate.
  }

  ASSERT_EQ(arena.per_col_bits.size(), static_cast<size_t>(B) * M);
  ASSERT_EQ(arena.per_col_seeds.size(), static_cast<size_t>(B) * M);
  ASSERT_EQ(arena.per_col_word_off.size(), static_cast<size_t>(B) * M);

  uint64_t cursor_words = 0;
  for (uint32_t b = 0; b < B; b++) {
    for (uint32_t c = 0; c < M; c++) {
      size_t idx = static_cast<size_t>(b) * M + c;
      ASSERT_EQ(arena.per_col_word_off[idx], cursor_words)
          << "per_col_word_off mismatch at (b=" << b << ",c=" << c << ")";
      cursor_words += (arena.per_col_bits[idx] + 63u) / 64u;
    }
  }

  ASSERT_EQ(arena.solution_bits.size(), cursor_words + 1)
      << "missing trailing guard word";
}

// Builds a MultisetCsf from column-major values, checks the arena invariants on
// every group, and verifies every key round-trips to its row.
template <typename T>
void buildCheckQuery(const std::vector<std::vector<T>> &columns) {
  const size_t num_rows = columns.empty() ? 0 : columns[0].size();
  auto keys = makeKeys(num_rows);
  MultisetConfig config;
  config.verbose = false;
  auto csf = constructMultisetCsf<T>(keys, columns, config);

  for (const auto &group : csf->groups()) {
    checkArenaInvariants<T>(group);
  }

  for (size_t r = 0; r < num_rows; r++) {
    std::vector<T> expected;
    expected.reserve(columns.size());
    for (const auto &col : columns) {
      expected.push_back(col[r]);
    }
    EXPECT_EQ(csf->query(keys[r]), expected) << "row " << r;
  }
}

}  // namespace

// One no-filter group: every column shares the full key set, so all columns
// pack into one interleaved arena and query must round-trip through the decode.
TEST(ConstructMultisetTest, InterleavedArenaQueriesDecodeCorrectly) {
  const size_t num_rows = 500, num_cols = 6;
  std::vector<std::vector<uint32_t>> columns(num_cols,
                                             std::vector<uint32_t>(num_rows));
  for (size_t c = 0; c < num_cols; c++) {
    for (size_t r = 0; r < num_rows; r++) {
      columns[c][r] = static_cast<uint32_t>((r * 7 + c * 13) % 50);
    }
  }
  buildCheckQuery<uint32_t>(columns);
}

// Columns with very different per-column target bucket counts (constant /
// low-entropy / all-distinct) exercise subgrouping by bucket count; the arena
// invariants and round-trip must hold across the resulting groups.
TEST(ConstructMultisetTest, HeterogeneousBucketCountsRoundTrip) {
  const size_t num_rows = 800;
  std::vector<std::vector<uint32_t>> columns(3, std::vector<uint32_t>(num_rows));
  for (size_t r = 0; r < num_rows; r++) {
    columns[0][r] = 5;                              // constant
    columns[1][r] = static_cast<uint32_t>(r % 4);   // low entropy
    columns[2][r] = static_cast<uint32_t>(r);       // all distinct
  }
  buildCheckQuery<uint32_t>(columns);
}

}  // namespace caramel::tests
