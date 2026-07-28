// Access-pattern probe: measures the query-side memory + decode cost of the
// current per-column layout against the proposed row-fused layout, at the real
// geometry of the 1M x 100 MovieLens index, without building either.
//
// Current layout : per bucket, M column ranges; each column read at 3 random
//                  bit offsets inside its own range.
// Fused layout   : per bucket, one range; 3 random reads of a W-bit window,
//                  XORed, then M codewords decoded sequentially out of it.
//
// Both variants then run M canonical decodes against per-column symbol tables
// of the real size, so the decode tail is charged to both.

#include <src/construct/CsfCodebook.h>
#include <src/construct/ConstructUtils.h>
#include <src/construct/CsfQueryCore.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

using namespace caramel;

namespace {

constexpr uint32_t M = 100;             // columns
constexpr uint32_t MAX_CODELENGTH = 20; // per column
constexpr uint32_t NUM_SYMBOLS = 4800;  // per column
constexpr double AVG_CODELENGTH = 8.9;  // bits, from the real index
constexpr double DELTA = 1.089;

struct Column {
  CanonicalDecodeTables tables;
  std::vector<uint32_t> symbols;
};

// A plausible canonical length distribution with the right mean/max, only so
// the decode does representative work.
std::vector<uint32_t> makeLengthCounts() {
  std::vector<uint32_t> counts(MAX_CODELENGTH + 1, 0);
  uint32_t remaining = NUM_SYMBOLS;
  double capacity = 2.0;
  for (uint32_t l = 1; l <= MAX_CODELENGTH && remaining; l++) {
    double target = (l < AVG_CODELENGTH) ? capacity * 0.02 : capacity * 0.5;
    uint32_t take = static_cast<uint32_t>(target);
    if (take > remaining || l == MAX_CODELENGTH) take = remaining;
    if (take > capacity) take = static_cast<uint32_t>(capacity);
    counts[l] = take;
    remaining -= take;
    capacity = (capacity - take) * 2.0;
  }
  return counts;
}

inline uint64_t readWindow(const uint64_t *arr, uint64_t bit_pos,
                           uint32_t width) {
  const uint64_t w = bit_pos >> 6;
  const int b = static_cast<int>(bit_pos & 63);
  const int l = 64 - static_cast<int>(width);
  if (b <= l) return arr[w] << b >> l;
  return (arr[w] << b >> l) | (arr[w + 1] >> (64 - b + l));
}

} // namespace

int main(int argc, char **argv) {
  const size_t num_buckets = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 2391;
  const size_t num_keys = 1000000;
  const size_t num_queries = 4000;
  const size_t reps = 15;

  std::mt19937_64 rng(1234);

  std::vector<Column> cols(M);
  auto counts = makeLengthCounts();
  for (auto &c : cols) {
    c.tables.build(counts, NUM_SYMBOLS, MAX_CODELENGTH);
    c.symbols.resize(NUM_SYMBOLS);
    for (auto &s : c.symbols) s = static_cast<uint32_t>(rng());
  }

  // Geometry: keys/bucket rows of M codewords, DELTA-expanded, per column.
  const size_t keys_per_bucket = num_keys / num_buckets;
  const uint64_t col_range_bits = static_cast<uint64_t>(
      keys_per_bucket * AVG_CODELENGTH * DELTA + MAX_CODELENGTH);
  const uint64_t col_range_words = (col_range_bits + 63) / 64;
  const uint64_t bucket_words_split = col_range_words * M;

  const uint32_t W = static_cast<uint32_t>(M * AVG_CODELENGTH * 1.12); // max row
  const uint64_t fused_range_bits =
      static_cast<uint64_t>(keys_per_bucket * M * AVG_CODELENGTH * DELTA) + W;
  const uint64_t fused_range_words = (fused_range_bits + 63) / 64;

  std::vector<uint64_t> arena_split(bucket_words_split * num_buckets + 8);
  std::vector<uint64_t> arena_fused(fused_range_words * num_buckets + 8);
  for (auto &w : arena_split) w = rng();
  for (auto &w : arena_fused) w = rng();

  std::printf("buckets=%zu keys/bucket=%zu col_range=%llu bits  "
              "split_arena=%.1fMB fused_arena=%.1fMB W=%u\n",
              num_buckets, keys_per_bucket,
              static_cast<unsigned long long>(col_range_bits),
              arena_split.size() * 8 / 1e6, arena_fused.size() * 8 / 1e6, W);

  std::vector<uint32_t> bucket_pick(num_queries);
  for (auto &b : bucket_pick) b = rng() % num_buckets;
  std::vector<uint64_t> sig_lo(num_queries), sig_hi(num_queries);
  for (size_t i = 0; i < num_queries; i++) {
    sig_lo[i] = rng();
    sig_hi[i] = rng();
  }

  uint32_t out[M];
  uint64_t checksum = 0;

  auto bench = [&](const char *name, auto &&fn) {
    std::vector<double> ns;
    for (size_t r = 0; r < reps; r++) {
      auto t0 = std::chrono::steady_clock::now();
      for (size_t i = 0; i < num_queries; i++) fn(i);
      auto dt = std::chrono::steady_clock::now() - t0;
      if (r) ns.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(dt)
                              .count() /
                          static_cast<double>(num_queries));
    }
    std::sort(ns.begin(), ns.end());
    std::printf("%-14s %8.1f ns/query\n", name, ns[ns.size() / 2]);
  };

  bench("split", [&](size_t i) {
    const __uint128_t sig =
        (static_cast<__uint128_t>(sig_hi[i]) << 64) | sig_lo[i];
    const uint64_t *base = arena_split.data() +
                           static_cast<uint64_t>(bucket_pick[i]) * bucket_words_split;
    const uint32_t num_vars =
        static_cast<uint32_t>(col_range_bits - MAX_CODELENGTH);
    constexpr size_t CHUNK = 24;
    uint64_t e[CHUNK][3];
    for (size_t b = 0; b < M; b += CHUNK) {
      size_t end = std::min<size_t>(b + CHUNK, M);
      for (size_t c = b; c < end; c++) {
        const uint64_t *arr = base + c * col_range_words;
        signatureToEquation(sig, c, num_vars, e[c - b]);
        __builtin_prefetch(arr + (e[c - b][0] >> 6));
        __builtin_prefetch(arr + (e[c - b][1] >> 6));
        __builtin_prefetch(arr + (e[c - b][2] >> 6));
      }
      for (size_t c = b; c < end; c++) {
        const uint64_t *arr = base + c * col_range_words;
        uint64_t v = gatherEncodedValue(arr, e[c - b], MAX_CODELENGTH);
        out[c] = canonicalDecodeBranchless<uint32_t>(v, cols[c].tables,
                                                     cols[c].symbols);
      }
    }
    checksum += out[0] + out[M - 1];
  });

  bench("fused", [&](size_t i) {
    const __uint128_t sig =
        (static_cast<__uint128_t>(sig_hi[i]) << 64) | sig_lo[i];
    const uint64_t *base =
        arena_fused.data() + static_cast<uint64_t>(bucket_pick[i]) * fused_range_words;
    uint64_t e[3];
    signatureToEquation(sig, 0, fused_range_bits - W, e);
    const size_t nw = (W + 63) / 64;
    uint64_t buf[64] = {0};
    for (int k = 0; k < 3; k++) {
      const uint64_t *src = base + (e[k] >> 6);
      const int s = static_cast<int>(e[k] & 63);
      __builtin_prefetch(src);
      __builtin_prefetch(src + 8);
      for (size_t j = 0; j <= nw; j++) {
        uint64_t lo = src[j] >> s;
        uint64_t hi = s ? (src[j + 1] << (64 - s)) : 0;
        buf[j] ^= lo | hi;
      }
    }
    uint32_t off = 0;
    for (uint32_t c = 0; c < M; c++) {
      uint64_t v = readWindow(buf, off, MAX_CODELENGTH);
      uint32_t length = 1;
      for (uint32_t l = 1; l < MAX_CODELENGTH; l++)
        length += (v >= cols[c].tables.limit[l]);
      int64_t idx = cols[c].tables.index_bias[length] +
                    static_cast<int64_t>(v >> (MAX_CODELENGTH - length));
      if (idx < 0 || idx >= static_cast<int64_t>(NUM_SYMBOLS)) idx = 0;
      out[c] = cols[c].symbols[idx];
      off += length;
    }
    checksum += out[0] + out[M - 1];
  });

  std::printf("checksum %llu\n", static_cast<unsigned long long>(checksum));
  return 0;
}
