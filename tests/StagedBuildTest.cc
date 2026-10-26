#include <gtest/gtest.h>
#include <random>
#include <set>
#include <src/build/Build.h>
#include <src/build/Bytes.h>
#include <src/construct/Csf.h>
#include <src/construct/multiset/PackedCsf.h>

namespace caramel::tests {

using namespace caramel::build;

namespace {

std::vector<std::string> keyStrings(size_t n) {
  std::vector<std::string> keys;
  for (size_t i = 0; i < n; i++) {
    keys.push_back("key_" + std::to_string(i));
  }
  return keys;
}

Signatures makeKeys(size_t n) { return hashKeys(keyStrings(n)); }

constexpr double STARVED_OVERHEAD = 0.85;

// Mostly zeros, so filters have something to do.
std::vector<uint32_t> skewedValues(size_t n, uint32_t seed) {
  std::mt19937 rng(seed);
  std::vector<uint32_t> values(n);
  for (auto &v : values) {
    v = rng() % 5 == 0 ? 1 + rng() % 40 : 0;
  }
  return values;
}

// One chunk: the contiguous input rows [begin, end).
struct Chunk {
  Signatures keys;
  Rows<uint32_t> rows;
};

using ChunkFn = std::function<Chunk(uint64_t begin, uint64_t end)>;

// Runs every step the way a cluster would: per-chunk steps, merges in chunk
// order, and a serialization round trip at every process boundary.
Index<uint32_t> distributedBuild(const BuildSpec &spec, uint64_t n,
                                 const ChunkFn &chunk_of, uint32_t num_chunks,
                                 uint32_t num_partitions) {
  std::vector<Chunk> chunks;
  for (uint32_t s = 0; s < num_chunks; s++) {
    chunks.push_back(chunk_of(n * s / num_chunks, n * (s + 1) / num_chunks));
  }
  auto countAll = [&](const Filters<uint32_t> *filters) {
    Counts<uint32_t> merged;
    for (size_t s = 0; s < chunks.size(); s++) {
      auto part = fromBytes<Counts<uint32_t>>(
          toBytes(countValues(spec, chunks[s].keys, chunks[s].rows, filters)));
      if (s == 0) {
        merged = std::move(part);
      } else {
        merged.merge(part);
      }
    }
    return merged;
  };

  Counts<uint32_t> counts = countAll(nullptr);
  Filters<uint32_t> filters;
  if (spec.filter.kind != FilterType::None) {
    auto choice = fromBytes<FilterChoice<uint32_t>>(toBytes(chooseFilters(spec, counts)));
    Exceptions exceptions;
    for (auto &c : chunks) {
      exceptions.append(fromBytes<Exceptions>(
          toBytes(findExceptions(choice, c.keys, c.rows))));
    }
    filters = fromBytes<Filters<uint32_t>>(toBytes(buildFilters(choice, exceptions)));
    if (!filters.empty()) {
      counts = countAll(&filters);
    }
  }
  auto plan = fromBytes<Plan<uint32_t>>(toBytes(makePlan(spec, counts, filters)));
  auto partitions = plan.partitions(num_partitions);

  std::vector<std::vector<Piece<uint32_t>>> per_partition(partitions.size());
  for (auto &c : chunks) {
    auto pieces = split(plan, c.keys, c.rows, partitions);
    for (size_t j = 0; j < partitions.size(); j++) {
      pieces[j].pack(plan);
      per_partition[j].push_back(fromBytes<Piece<uint32_t>>(toBytes(pieces[j])));
    }
  }
  std::vector<SolvedPartition> solved;
  for (auto &parts : per_partition) {
    solved.push_back(fromBytes<SolvedPartition>(
        toBytes(solve(plan, Piece<uint32_t>::merge(std::move(parts))))));
  }
  return combine(std::move(plan), std::move(solved));
}

std::string indexBytes(const Index<uint32_t> &index) {
  std::string out = toBytes(index.groups);
  if (index.shared_codebook) {
    out += toBytes(*index.shared_codebook);
  }
  return out + std::to_string(index.max_row_bits);
}

struct Case {
  std::string name;
  BuildSpec spec;
  enum Layout { Single, Dense, Ragged } layout;
};

std::vector<Case> cases() {
  auto spec = [](FilterType f, bool shared_filter = false,
                 CodebookMode codebook = CodebookMode::Auto) {
    BuildSpec s;
    s.filter = f;
    s.shared_filter = shared_filter;
    s.codebook = codebook;
    return s;
  };
  BuildSpec packed;
  packed.encoding = Encoding::Packed;
  auto grown = [](BuildSpec s, double overhead) {
    s.ribbon_overhead = overhead;
    return s;
  };
  return {
      {"CsfCellsGrow", grown(spec({}), STARVED_OVERHEAD), Case::Single},
      {"MultisetCellsGrow", grown(spec({FilterType::Auto, 0, 0}), STARVED_OVERHEAD),
       Case::Dense},
      {"PackedCellsGrow", grown(packed, STARVED_OVERHEAD), Case::Ragged},
      {"CsfNoFilter", spec({}), Case::Single},
      {"CsfBloom", spec({FilterType::Bloom, 8, 3}), Case::Single},
      {"CsfXor", spec({FilterType::Xor, 8, 0}), Case::Single},
      {"CsfBinaryFuse", spec({FilterType::BinaryFuse, 8, 0}), Case::Single},
      {"CsfAuto", spec({FilterType::Auto, 0, 0}), Case::Single},
      {"MultisetNoFilter", spec({}), Case::Dense},
      {"MultisetAuto", spec({FilterType::Auto, 0, 0}), Case::Dense},
      {"MultisetSharedFilter", spec({FilterType::Xor, 6, 0}, true), Case::Dense},
      {"MultisetSharedEverything",
       spec({FilterType::Bloom, 6, 2}, true, CodebookMode::Shared), Case::Dense},
      {"MultisetPerColumnCodebooks", spec({}, false, CodebookMode::PerColumn),
       Case::Dense},
      {"RaggedAuto", spec({FilterType::Auto, 0, 0}), Case::Ragged},
      {"Packed", packed, Case::Ragged},
  };
}

} // namespace

class DistributedBuildTest : public ::testing::TestWithParam<Case> {};

// A build split over chunks and partitions, with every step output
// serialized in between, is byte-identical to a single-process build.
TEST_P(DistributedBuildTest, MatchesLocalBuildByteForByte) {
  const Case &c = GetParam();
  const uint64_t n = 60000;
  const uint32_t cols = c.layout == Case::Single ? 1 : 5;
  Signatures keys = makeKeys(n);
  std::vector<uint32_t> values = skewedValues(n * cols, 7);
  std::vector<uint64_t> offsets(n + 1, 0);
  for (uint64_t r = 0; r < n; r++) {
    offsets[r + 1] = offsets[r] + 1 + r % cols;
  }

  auto rows_of = [&](uint64_t begin, uint64_t end) {
    if (c.layout == Case::Ragged) {
      return Rows<uint32_t>::ragged(values.data(), offsets.data() + begin,
                                    end - begin);
    }
    return Rows<uint32_t>::rowMajor(values.data() + begin * cols, end - begin,
                                    cols);
  };
  ChunkFn chunk_of = [&](uint64_t begin, uint64_t end) {
    return Chunk{Signatures(keys.begin() + begin, keys.begin() + end),
                      rows_of(begin, end)};
  };

  auto local = buildLocal(c.spec, keys, rows_of(0, n));
  auto distributed = distributedBuild(c.spec, n, chunk_of, 3, 4);
  EXPECT_EQ(indexBytes(local), indexBytes(distributed));
  EXPECT_EQ(indexBytes(local), indexBytes(distributedBuild(c.spec, n, chunk_of, 1, 1)));
}

INSTANTIATE_TEST_SUITE_P(AllIndexTypes, DistributedBuildTest,
                         ::testing::ValuesIn(cases()),
                         [](const auto &info) { return info.param.name; });

// Cells that no seed solves are grown and re-solved after the rest of the
// partition, which moves every cell after them: solved cells must survive the
// move and grown ones must decode. Solvability falls off sharply near one
// solution bit per equation; at this overhead a few percent of these cells
// exhaust their seeds.
TEST(StagedBuildTest, CellsThatGrowStillDecode) {
  constexpr double overhead = 1.005;
  const uint64_t n = 200000;
  const auto strings = keyStrings(n);
  const Signatures keys = hashKeys(strings);
  const auto values = skewedValues(n, 7);
  const auto rows = Rows<uint32_t>::single(values);
  BuildSpec spec;
  spec.ribbon_overhead = overhead;
  auto plan = makePlan(spec, countValues(spec, keys, rows));
  const auto &dict = plan.codebooks[0]->codedict;

  std::vector<SolvedPartition> solved;
  uint64_t grown = 0, cells = 0;
  for (const auto &piece : split(plan, keys, rows, plan.partitions(3))) {
    solved.push_back(solve(plan, piece));
    for (uint32_t b = 0; b < piece.partition.size(); b++, cells++) {
      uint64_t eq = 0;
      for (uint64_t i = piece.offsets[b]; i < piece.offsets[b + 1]; i++) {
        eq += dict.at(values[piece.rows[i]]).length;
      }
      grown += solved.back().vars[b] != ribbonStarts(eq, overhead);
    }
  }
  ASSERT_GT(grown, 0u);
  ASSERT_LT(grown, cells);

  Csf<uint32_t> csf(combine(std::move(plan), std::move(solved)));
  for (uint64_t i = 0; i < n; i++) {
    ASSERT_EQ(csf.query(strings[i]), values[i]) << "key " << i;
  }
}

TEST(StagedBuildTest, PackedDecodesWhenEveryCellGrows) {
  const uint64_t n = 10000;
  const uint32_t cols = 4;
  const auto strings = keyStrings(n);
  const Signatures keys = hashKeys(strings);
  const auto values = skewedValues(n * cols, 3);
  BuildSpec spec;
  spec.ribbon_overhead = STARVED_OVERHEAD;
  spec.encoding = Encoding::Packed;
  PackedCsf<uint32_t> packed(
      buildLocal(spec, keys, Rows<uint32_t>::rowMajor(values.data(), n, cols)));
  for (uint64_t i = 0; i < n; i++) {
    std::vector<uint32_t> want(values.begin() + i * cols,
                               values.begin() + (i + 1) * cols);
    auto got = packed.query(strings[i]);
    std::sort(got.begin(), got.end());
    std::sort(want.begin(), want.end());
    ASSERT_EQ(got, want) << "key " << i;
  }
}

TEST(StagedBuildTest, FilterHashIsHighHalfOfSignature) {
  for (int i = 0; i < 100; i++) {
    std::string key = "key" + std::to_string(i) + std::string(i % 20, 'x');
    EXPECT_EQ(filterHash(key.data(), key.size()),
              static_cast<uint64_t>(hashKey(key) >> 64));
  }
}

// Keys under 16 bytes get a Feistel permutation of their bytes, read with
// overlapping loads that differ by length. Two same-length keys must never
// share a signature: every key of up to 2 bytes, and every single-byte change
// of longer short keys.
TEST(StagedBuildTest, ShortKeysNeverCollide) {
  std::set<__uint128_t> exhaustive;
  std::string key;
  exhaustive.insert(hashKey(key));
  for (int a = 0; a < 256; a++) {
    exhaustive.insert(hashKey(std::string(1, char(a))));
    for (int b = 0; b < 256; b++) {
      exhaustive.insert(hashKey(std::string{char(a), char(b)}));
    }
  }
  EXPECT_EQ(exhaustive.size(), 1u + 256 + 256 * 256);

  for (size_t len = 3; len < 16; len++) {
    const std::string base(len, 'k');
    std::set<__uint128_t> sigs = {hashKey(base)};
    for (size_t pos = 0; pos < len; pos++) {
      for (int v = 0; v < 256; v++) {
        if (v != 'k') {
          std::string k = base;
          k[pos] = char(v);
          sigs.insert(hashKey(k));
        }
      }
    }
    EXPECT_EQ(sigs.size(), 1 + len * 255) << "length " << len;
  }
}

TEST(StagedBuildTest, SequentialKeysFillBucketsEvenly) {
  // Short keys (< 16 bytes) and long keys use different hash functions.
  for (const std::string pad : {"", "-a-long-suffix"}) {
    const uint32_t num_buckets = 1000;
    std::vector<uint32_t> counts(num_buckets, 0);
    for (int i = 0; i < 1000000; i++) {
      counts[getBucketID(hashKey("key" + std::to_string(i) + pad), num_buckets)]++;
    }
    // Mean 1000, standard deviation ~32.
    for (uint32_t c : counts) {
      ASSERT_GT(c, 840u);
      ASSERT_LT(c, 1160u);
    }
  }
}

TEST(StagedBuildTest, BucketIdsSpread) {
  std::vector<__uint128_t> signatures;
  for (int i = 0; i < 100; i++) {
    signatures.push_back(hashKey("k" + std::to_string(i)));
  }
  for (uint32_t num_buckets = 10; num_buckets < 20; num_buckets++) {
    std::set<uint32_t> unique_buckets;
    for (auto signature : signatures) {
      uint32_t bucket_id = getBucketID(signature, num_buckets);
      unique_buckets.insert(bucket_id);
      ASSERT_LT(bucket_id, num_buckets);
    }
    ASSERT_GE(unique_buckets.size(), num_buckets / 2);
  }
}

TEST(StagedBuildTest, RowViewsAgree) {
  std::vector<std::vector<uint32_t>> cols = {{1, 2, 3}, {4, 5, 6}};
  std::vector<uint32_t> row_major = {1, 4, 2, 5, 3, 6};
  std::vector<uint32_t> flat = {1, 4, 2, 3, 6};
  std::vector<uint64_t> offsets = {0, 2, 3, 5};
  auto a = Rows<uint32_t>::columns(cols);
  auto b = Rows<uint32_t>::rowMajor(row_major.data(), 3, 2);
  auto r = Rows<uint32_t>::ragged(flat.data(), offsets.data(), 3);
  EXPECT_EQ(r.numCols(), 2u);
  for (uint64_t row = 0; row < 3; row++) {
    for (uint32_t c = 0; c < 2; c++) {
      EXPECT_EQ(a.at(row, c), b.at(row, c));
      if (r.present(row, c)) {
        EXPECT_EQ(r.at(row, c), a.at(row, c));
      }
    }
  }
  EXPECT_FALSE(r.present(1, 1));
}

TEST(StagedBuildTest, SplitGroupsByBucketInInputOrder) {
  Signatures keys = makeKeys(50000);
  auto values = skewedValues(keys.size(), 3);
  auto rows = Rows<uint32_t>::single(values);
  BuildSpec spec;
  auto plan = makePlan(spec, countValues(spec, keys, rows));
  const uint32_t nb = plan.groups[0].num_buckets;
  auto pieces = split(plan, keys, rows, {{0, 0, nb}});
  const auto &piece = pieces[0];
  ASSERT_EQ(piece.size(), keys.size());
  for (uint32_t b = 0; b < nb; b++) {
    for (uint64_t i = piece.offsets[b]; i < piece.offsets[b + 1]; i++) {
      EXPECT_EQ(getBucketID(piece.sigs[i], nb), b);
      EXPECT_EQ(piece.sigs[i], keys[piece.rows[i]]);
      if (i > piece.offsets[b]) {
        EXPECT_LT(piece.rows[i - 1], piece.rows[i]);
      }
    }
  }
}

// Each column draws from its own `distinct` values, offset by column * shift:
// shift 0 gives every column the same distribution, a large shift disjoint ones.
bool autoPicksShared(uint64_t n, uint32_t distinct, uint32_t shift) {
  const uint32_t cols = 8;
  Signatures keys = makeKeys(n);
  std::mt19937 rng(5);
  std::vector<uint32_t> values(n * cols);
  for (uint64_t i = 0; i < values.size(); i++) {
    values[i] = static_cast<uint32_t>(i % cols) * shift + rng() % distinct;
  }
  auto rows = Rows<uint32_t>::rowMajor(values.data(), n, cols);
  BuildSpec spec;
  return makePlan(spec, countValues(spec, keys, rows)).shared_codebook;
}

TEST(StagedBuildTest, AutoCodebookSharesTablesWhenColumnsAlike) {
  // Few rows per distinct value: eight symbol tables would cost more than
  // per-column codes could save.
  EXPECT_TRUE(autoPicksShared(4000, 2000, 0));
  EXPECT_TRUE(autoPicksShared(4000, 2000, 100));
}

TEST(StagedBuildTest, AutoCodebookKeepsColumnsApartWhenTheyDiffer) {
  // Disjoint columns under one codebook pay log2(cols) extra bits per code.
  EXPECT_FALSE(autoPicksShared(50000, 16, 1000));
}

TEST(StagedBuildTest, AutoCodebookIsPerColumnForOneColumn) {
  Signatures keys = makeKeys(20000);
  auto values = skewedValues(keys.size(), 3);
  auto rows = Rows<uint32_t>::single(values);
  BuildSpec spec;
  EXPECT_FALSE(makePlan(spec, countValues(spec, keys, rows)).shared_codebook);
}

TEST(StagedBuildTest, StatsMergeEqualsWholeFit) {
  Signatures keys = makeKeys(4000);
  auto values = skewedValues(keys.size(), 5);
  BuildSpec spec;
  auto whole = countValues(spec, keys, Rows<uint32_t>::single(values));
  Signatures head(keys.begin(), keys.begin() + 1500), tail(keys.begin() + 1500, keys.end());
  auto merged = countValues(spec, head, Rows<uint32_t>::rowMajor(values.data(), 1500, 1));
  merged.merge(countValues(spec, tail, Rows<uint32_t>::rowMajor(values.data() + 1500, 2500, 1)));
  EXPECT_EQ(merged.num_rows, whole.num_rows);
  EXPECT_EQ(merged.cols[0].count, whole.cols[0].count);
  EXPECT_EQ(merged.cols[0].freq, whole.cols[0].freq);
}

TEST(StagedBuildTest, SharedFilterGroupsColumnsByMostCommonValue) {
  Signatures keys = makeKeys(30000);
  // Columns 0 and 2 are mostly 0, column 1 mostly 7.
  std::vector<uint32_t> values(keys.size() * 3);
  std::mt19937 rng(11);
  for (size_t r = 0; r < keys.size(); r++) {
    values[r * 3 + 0] = rng() % 4 == 0 ? rng() % 9 : 0;
    values[r * 3 + 1] = rng() % 4 == 0 ? rng() % 9 : 7;
    values[r * 3 + 2] = rng() % 4 == 0 ? rng() % 9 : 0;
  }
  BuildSpec spec;
  spec.filter = {FilterType::Xor, 8, 0};
  spec.shared_filter = true;
  auto plan = chooseFilters(spec, countValues(spec, keys, Rows<uint32_t>::rowMajor(values.data(), keys.size(), 3)));
  ASSERT_EQ(plan.specs.size(), 2u);
  EXPECT_EQ(plan.specs[0].columns, (std::vector<uint32_t>{0, 2}));
  EXPECT_EQ(plan.specs[1].columns, (std::vector<uint32_t>{1}));
  EXPECT_EQ(plan.specs[1].most_common_value, 7u);
}

TEST(StagedBuildTest, DuplicateKeyIsRejected) {
  Signatures keys = hashKeys(std::vector<std::string>{"a", "b", "c", "a"});
  std::vector<uint32_t> values = {1, 2, 3, 4};
  BuildSpec spec;
  EXPECT_THROW(buildLocal(spec, keys, Rows<uint32_t>::single(values)),
               std::runtime_error);
}

TEST(StagedBuildTest, SolvedPartitionsMustTileTheGroup) {
  Signatures keys = makeKeys(30000);
  auto values = skewedValues(keys.size(), 9);
  auto rows = Rows<uint32_t>::single(values);
  BuildSpec spec;
  auto plan = makePlan(spec, countValues(spec, keys, rows));
  auto partitions = plan.partitions(3);
  ASSERT_EQ(partitions.size(), 3u);
  auto pieces = split(plan, keys, rows, partitions);
  std::vector<SolvedPartition> solved = {solve(plan, pieces[0]), solve(plan, pieces[2])};
  EXPECT_THROW(combine(plan, solved), std::invalid_argument);
}

} // namespace caramel::tests
