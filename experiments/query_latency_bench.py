"""Measure per-query latency.

Uses batch timing (time whole loop / N) for accurate averages,
and per-call timing for distributions.
"""

import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import carameldb
from carameldb import BinaryFuseFilterConfig
from data_gen import gen_alpha_values, gen_keys

N = 1_000_000
NUM_QUERY_KEYS = 200_000
SEED = 42
ITERATIONS = 10


def bench_batch(fn, keys, iterations=ITERATIONS):
    """Time N calls in a tight loop, report per-call ns. Best of `iterations`."""
    for key in keys[:1000]:
        fn(key)

    results = []
    for _ in range(iterations):
        t0 = time.perf_counter_ns()
        for key in keys:
            fn(key)
        t1 = time.perf_counter_ns()
        results.append((t1 - t0) / len(keys))
    return min(results)


def bench_individual(fn, keys):
    """Time each call individually, return array of per-call ns."""
    for key in keys[:1000]:
        fn(key)

    times = np.empty(len(keys), dtype=np.int64)
    for i, key in enumerate(keys):
        t0 = time.perf_counter_ns()
        fn(key)
        t1 = time.perf_counter_ns()
        times[i] = t1 - t0
    return times


def main():
    print(f"N={N:,} keys, querying {NUM_QUERY_KEYS:,} keys\n")

    keys = gen_keys(N, seed=SEED)
    rng = np.random.RandomState(SEED)
    indices = rng.choice(N, size=NUM_QUERY_KEYS, replace=False)
    query_keys = [keys[i] for i in indices]
    query_keys_bytes = [
        k.encode("utf-8") if isinstance(k, str) else k for k in query_keys
    ]

    configs = [
        ("uniform_100, alpha=0.5", "uniform_100", 0.5),
        ("unique, alpha=0.2", "unique", 0.2),
        ("zipfian, alpha=0.85", "zipfian", 0.85),
    ]

    for label, dist, alpha in configs:
        print(f"--- {label} ---")
        values = gen_alpha_values(N, alpha, seed=SEED, minority_dist=dist)
        config = BinaryFuseFilterConfig(fingerprint_bits=8)
        csf = carameldb.Caramel(keys, values, prefilter=config, verbose=False)

        csf_raw = csf

        # C++ internal loop
        _, cpp_ns = csf_raw.benchmark_queries(query_keys, 10)

        # Batch timing (most accurate)
        str_ns = bench_batch(csf_raw.query, query_keys)
        bytes_ns = bench_batch(csf_raw.query, query_keys_bytes)

        # Per-call distribution
        times_str = bench_individual(csf_raw.query, query_keys)

        overhead = str_ns - cpp_ns

        print(f"  C++ internal loop:     {cpp_ns:6.1f} ns/query")
        print(f"  Python query(str):     {str_ns:6.1f} ns/query")
        print(f"  Python query(bytes):   {bytes_ns:6.1f} ns/query")
        print(f"  Binding overhead:      {overhead:6.1f} ns/query")
        print(
            f"  Per-call distribution: median={np.median(times_str):.0f}  "
            f"mean={np.mean(times_str):.0f}  "
            f"p5={np.percentile(times_str, 5):.0f}  "
            f"p95={np.percentile(times_str, 95):.0f} ns"
        )
        print()


if __name__ == "__main__":
    main()
