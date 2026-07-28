// Standalone query-latency harness for MultisetCsf<uint32_t>.
//
// Loads a CSF directory saved by the Python bindings (type_id 1101) and times
// queries from C++, so numbers are free of the Python binding's per-call list
// construction. Keys are the synthetic uint32 arange used by
// scripts/benchmark_multiset.py: key i is the 4 raw bytes of uint32 i.
//
//   ./QueryBench <csf_dir> <num_keys_in_dataset> [num_queries] [reps]

#include <src/construct/multiset/MultisetCsf.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <random>
#include <vector>

int main(int argc, char **argv) {
  if (argc < 3) {
    std::fprintf(stderr,
                 "usage: %s <csf_dir> <num_keys> [num_queries=2000] [reps=11]\n",
                 argv[0]);
    return 1;
  }
  const std::string dir = argv[1];
  const uint64_t num_keys = std::strtoull(argv[2], nullptr, 10);
  const size_t num_queries = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 2000;
  const size_t reps = argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 11;

  auto csf = caramel::MultisetCsf<uint32_t>::load(dir, 1101);

  if (getenv("CARAMEL_STATS")) {
    for (const auto &g : csf->groups()) {
      uint32_t min_cl = 999, max_cl = 0;
      size_t min_sym = SIZE_MAX, max_sym = 0;
      for (const auto &c : g.columns) {
        min_cl = std::min(min_cl, c.max_codelength);
        max_cl = std::max(max_cl, c.max_codelength);
        min_sym = std::min(min_sym, c.codebook->ordered_symbols.size());
        max_sym = std::max(max_sym, c.codebook->ordered_symbols.size());
      }
      std::printf("group cols=%zu buckets=%u arena_words=%zu "
                  "max_codelength=[%u,%u] symbols=[%zu,%zu] "
                  "bits_per_col_bucket=%u\n",
                  g.columns.size(), g.arena.num_buckets,
                  g.arena.solution_bits.size(), min_cl, max_cl, min_sym,
                  max_sym, g.arena.per_col_bits.empty() ? 0
                                                        : g.arena.per_col_bits[0]);
    }
  }

  std::mt19937_64 rng(42);
  std::uniform_int_distribution<uint32_t> pick(0, static_cast<uint32_t>(num_keys - 1));
  std::vector<uint32_t> query_keys(num_queries);
  for (auto &k : query_keys) {
    k = pick(rng);
  }

  uint64_t checksum = 0;

  auto time_median = [&](auto &&body) {
    std::vector<double> trial_ns;
    for (size_t r = 0; r < reps; r++) {
      auto start = std::chrono::steady_clock::now();
      for (size_t i = 0; i < num_queries; i++) {
        body(i);
      }
      auto elapsed = std::chrono::steady_clock::now() - start;
      double ns =
          std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
      if (r > 0) { // first rep is warmup
        trial_ns.push_back(ns / num_queries);
      }
    }
    std::sort(trial_ns.begin(), trial_ns.end());
    return trial_ns[trial_ns.size() / 2];
  };

  // Pre-change query path: one column at a time, no prefetch pipeline, and the
  // bit-at-a-time canonical decode. Kept here so both paths can be timed in one
  // binary against the same index.
  double legacy = time_median([&](size_t i) {
    const char *key = reinterpret_cast<const char *>(&query_keys[i]);
    std::vector<uint32_t> outputs(csf->totalCols());
    for (const auto &group : csf->groups()) {
      __uint128_t signature = caramel::hashKey(key, 4, group.hash_store_seed);
      uint32_t bucket_id =
          group.num_buckets()
              ? caramel::getBucketID(signature, group.num_buckets())
              : 0;
      for (size_t ci = 0; ci < group.columns.size(); ci++) {
        outputs[group.columns[ci].output_index] =
            group.queryColumn(ci, key, 4, signature, bucket_id);
      }
    }
    checksum += outputs[0] + outputs.back();
  });
  std::printf("legacy_ns_median %.1f\n", legacy);

  double median = time_median([&](size_t i) {
    auto out = csf->query(reinterpret_cast<const char *>(&query_keys[i]), 4,
                          /*parallelize=*/false);
    checksum += out[0] + out.back();
  });
  std::printf("query_ns_median %.1f\n", median);

  std::vector<uint32_t> reuse;
  double into = time_median([&](size_t i) {
    csf->queryInto(reinterpret_cast<const char *>(&query_keys[i]), 4, reuse);
    checksum += reuse[0] + reuse.back();
  });
  std::printf("query_into_ns_median %.1f\n", into);

  {
    std::vector<std::string> key_strings(num_queries);
    for (size_t i = 0; i < num_queries; i++) {
      key_strings[i].assign(reinterpret_cast<const char *>(&query_keys[i]), 4);
    }
    std::vector<uint32_t> batch_out;
    std::vector<double> batch_ns;
    for (size_t r = 0; r < reps; r++) {
      auto start = std::chrono::steady_clock::now();
      csf->queryBatch(key_strings, batch_out);
      auto elapsed = std::chrono::steady_clock::now() - start;
      checksum += batch_out[0];
      if (r > 0) {
        batch_ns.push_back(
            std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed)
                .count() /
            static_cast<double>(num_queries));
      }
    }
    std::sort(batch_ns.begin(), batch_ns.end());
    std::printf("batch_ns_median %.1f\n", batch_ns[batch_ns.size() / 2]);
  }

  std::printf("checksum %llu\n", static_cast<unsigned long long>(checksum));
  return 0;
}
