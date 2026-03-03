"""Query throughput benchmark for CaramelDB.

Measures ns/query over N=10M datasets with various distributions and alpha values.
Uses C++ benchmark_queries method for accurate timing (no Python overhead).

Each trial calls benchmark_queries with a random sample of keys from the dataset.
First 10 trials are warmup (discarded), remaining 100 are measured.
"""

import json
import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import carameldb
from carameldb import BinaryFuseFilterConfig
from data_gen import gen_alpha_values, gen_keys

N = 10_000_000
NUM_QUERY_KEYS = 50_000
NUM_ITERATIONS = 1  # iterations inside C++ per trial
WARMUP_TRIALS = 10
MEASURE_TRIALS = 100
SEED = 42

DISTRIBUTIONS = ["zipfian", "uniform_100", "unique"]
ALPHAS = [0.2, 0.5, 0.85]


def sample_query_keys(keys, n_sample, rng):
    indices = rng.choice(len(keys), size=n_sample, replace=False)
    return [keys[i] for i in indices]


def main():
    results = []

    total_trials = WARMUP_TRIALS + MEASURE_TRIALS
    print(f"Query Benchmark: N={N:,}, {NUM_QUERY_KEYS:,} query keys/trial, "
          f"{WARMUP_TRIALS} warmup + {MEASURE_TRIALS} measured trials")
    print("=" * 72)

    keys = gen_keys(N, seed=SEED)

    for dist in DISTRIBUTIONS:
        for alpha in ALPHAS:
            print(f"\n{dist} alpha={alpha}:")
            values = gen_alpha_values(N, alpha, seed=SEED, minority_dist=dist)

            config = BinaryFuseFilterConfig(fingerprint_bits=8)
            print("  Building CSF...", end="", flush=True)
            t0 = time.time()
            csf = carameldb.Caramel(keys, values, prefilter=config, verbose=False)
            build_time = time.time() - t0
            print(f" {build_time:.1f}s")

            rng = np.random.RandomState(SEED)
            trial_ns = []
            for t in range(total_trials):
                query_keys = sample_query_keys(keys, NUM_QUERY_KEYS, rng)
                _, ns_per_query = csf.benchmark_queries(query_keys, NUM_ITERATIONS)
                if t >= WARMUP_TRIALS:
                    trial_ns.append(ns_per_query)

            mean_ns = np.mean(trial_ns)
            std_ns = np.std(trial_ns)
            median_ns = np.median(trial_ns)
            p5 = np.percentile(trial_ns, 5)
            p95 = np.percentile(trial_ns, 95)
            print(f"  {median_ns:.1f} ns/query (mean={mean_ns:.1f}, "
                  f"std={std_ns:.1f}, p5={p5:.1f}, p95={p95:.1f})")

            results.append({
                "dist": dist,
                "alpha": alpha,
                "median_ns": round(median_ns, 2),
                "mean_ns": round(mean_ns, 2),
                "std_ns": round(std_ns, 2),
                "p5_ns": round(p5, 2),
                "p95_ns": round(p95, 2),
            })

    print("\n" + "=" * 72)
    print("\nSummary (ns/query):")
    print(f"{'dist':<15} {'alpha':<8} {'median':>8} {'mean':>8} {'std':>8}")
    print("-" * 50)
    for r in results:
        print(f"{r['dist']:<15} {r['alpha']:<8} "
              f"{r['median_ns']:>8.1f} {r['mean_ns']:>8.1f} {r['std_ns']:>8.1f}")

    out_path = os.path.join(os.path.dirname(__file__), "query_benchmark_results.json")
    with open(out_path, "w") as f:
        json.dump(results, f, indent=2)
    print(f"\nResults saved to {out_path}")


if __name__ == "__main__":
    main()
