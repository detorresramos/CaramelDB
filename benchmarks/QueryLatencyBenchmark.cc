// Isolates the pure-C++ per-query latency for a MultisetCsf<uint32_t>.
// Reads a CSF built by scripts/benchmark_multiset.py, hits it with random
// uint32 keys drawn from [0, N), and reports the timing distribution — same
// metrics as the Python benchmark so the two can be diffed directly.
//
// Usage:
//   QueryLatencyBenchmark <csf_dir> <N> [--warmup=200] [--count=1000]
//                        [--seed=42] [--parallel]

#include "src/construct/multiset/MultisetCsf.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace {

uint64_t percentile(const std::vector<uint64_t> &sorted, double p) {
  if (sorted.empty()) return 0;
  size_t idx = static_cast<size_t>(p * (sorted.size() - 1));
  return sorted[idx];
}

bool parseUlongFlag(const char *arg, const char *name, uint64_t &out) {
  size_t nlen = std::strlen(name);
  if (std::strncmp(arg, name, nlen) != 0 || arg[nlen] != '=') return false;
  out = std::strtoull(arg + nlen + 1, nullptr, 10);
  return true;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc < 3) {
    std::fprintf(stderr,
                 "Usage: %s <csf_dir> <N> [--warmup=200] [--count=1000] "
                 "[--seed=42] [--parallel]\n",
                 argv[0]);
    return 1;
  }
  std::string csf_path = argv[1];
  uint32_t universe = static_cast<uint32_t>(std::strtoull(argv[2], nullptr, 10));
  uint64_t warmup = 200;
  uint64_t count = 1000;
  uint64_t seed = 42;
  bool parallel = false;

  for (int i = 3; i < argc; i++) {
    if (parseUlongFlag(argv[i], "--warmup", warmup)) continue;
    if (parseUlongFlag(argv[i], "--count", count)) continue;
    if (parseUlongFlag(argv[i], "--seed", seed)) continue;
    if (std::strcmp(argv[i], "--parallel") == 0) {
      parallel = true;
      continue;
    }
    std::fprintf(stderr, "Unrecognized arg: %s\n", argv[i]);
    return 1;
  }

  std::fprintf(stderr,
               "Loading %s (universe=%u, warmup=%lu, count=%lu, seed=%lu, "
               "parallel=%d)...\n",
               csf_path.c_str(), universe,
               static_cast<unsigned long>(warmup),
               static_cast<unsigned long>(count),
               static_cast<unsigned long>(seed), (int)parallel);
  auto load_t0 = std::chrono::steady_clock::now();
  auto csf = caramel::MultisetCsf<uint32_t>::load(csf_path, 1101);
  auto load_ns = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - load_t0)
                     .count();
  std::fprintf(stderr, "Loaded in %ld ms. total_cols=%u\n",
               static_cast<long>(load_ns), csf->totalCols());

  std::mt19937_64 rng(seed);
  std::uniform_int_distribution<uint32_t> dist(0, universe - 1);
  std::vector<uint32_t> keys(warmup + count);
  for (auto &k : keys) k = dist(rng);

  // Warmup — untimed
  volatile uint32_t sink = 0;
  for (uint64_t i = 0; i < warmup; i++) {
    auto out = csf->query(reinterpret_cast<const char *>(&keys[i]), 4, parallel);
    sink ^= out.empty() ? 0 : out[0];
  }

  std::vector<uint64_t> ns(count);
  for (uint64_t i = 0; i < count; i++) {
    uint32_t key = keys[warmup + i];
    auto t0 = std::chrono::steady_clock::now();
    auto out = csf->query(reinterpret_cast<const char *>(&key), 4, parallel);
    auto t1 = std::chrono::steady_clock::now();
    ns[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0)
                .count();
    sink ^= out.empty() ? 0 : out[0];
  }
  (void)sink;

  double mean = 0.0;
  for (auto x : ns) mean += static_cast<double>(x);
  mean /= static_cast<double>(ns.size());

  std::vector<uint64_t> sorted = ns;
  std::sort(sorted.begin(), sorted.end());

  std::printf("{\n");
  std::printf("  \"csf_dir\": \"%s\",\n", csf_path.c_str());
  std::printf("  \"universe\": %u,\n", universe);
  std::printf("  \"warmup\": %lu,\n", static_cast<unsigned long>(warmup));
  std::printf("  \"count\": %lu,\n", static_cast<unsigned long>(count));
  std::printf("  \"parallel\": %s,\n", parallel ? "true" : "false");
  std::printf("  \"min_ns\": %lu,\n",
              static_cast<unsigned long>(sorted.front()));
  std::printf("  \"median_ns\": %lu,\n",
              static_cast<unsigned long>(percentile(sorted, 0.50)));
  std::printf("  \"mean_ns\": %.1f,\n", mean);
  std::printf("  \"p90_ns\": %lu,\n",
              static_cast<unsigned long>(percentile(sorted, 0.90)));
  std::printf("  \"p99_ns\": %lu,\n",
              static_cast<unsigned long>(percentile(sorted, 0.99)));
  std::printf("  \"max_ns\": %lu\n",
              static_cast<unsigned long>(sorted.back()));
  std::printf("}\n");
  return 0;
}
