"""Compare query_batch vs single query throughput."""

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


def main():
    print(f"N={N:,} keys, querying {NUM_QUERY_KEYS:,} keys\n")

    keys = gen_keys(N, seed=SEED)
    rng = np.random.RandomState(SEED)
    indices = rng.choice(N, size=NUM_QUERY_KEYS, replace=False)
    query_keys = [keys[i] for i in indices]

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

        # Single query loop (best of ITERATIONS)
        best_single = float("inf")
        for _ in range(ITERATIONS):
            t0 = time.perf_counter_ns()
            for key in query_keys:
                csf_raw.query(key)
            t1 = time.perf_counter_ns()
            best_single = min(best_single, (t1 - t0) / len(query_keys))

        # query_batch (best of ITERATIONS)
        best_batch = float("inf")
        for _ in range(ITERATIONS):
            t0 = time.perf_counter_ns()
            results = csf_raw.query_batch(query_keys)
            t1 = time.perf_counter_ns()
            best_batch = min(best_batch, (t1 - t0) / len(query_keys))

        print(f"  C++ internal loop:     {cpp_ns:6.1f} ns/query")
        print(f"  Python single query:   {best_single:6.1f} ns/query")
        print(f"  Python query_batch:    {best_batch:6.1f} ns/query")
        print(f"  Batch speedup:         {best_single / best_batch:5.2f}x")
        print()


if __name__ == "__main__":
    main()
