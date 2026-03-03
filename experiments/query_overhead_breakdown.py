"""Break down per-query overhead into components."""

import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import carameldb
from carameldb import BinaryFuseFilterConfig
from data_gen import gen_alpha_values, gen_keys

N = 1_000_000
NUM_QUERY_KEYS = 50_000
SEED = 42


def bench_loop(fn, keys, label):
    """Time individual calls, return median ns."""
    # Warmup
    for key in keys[:1000]:
        fn(key)

    times = []
    for key in keys:
        t0 = time.perf_counter_ns()
        fn(key)
        t1 = time.perf_counter_ns()
        times.append(t1 - t0)
    arr = np.array(times)
    print(f"  {label:30s}  median={np.median(arr):6.0f}  mean={np.mean(arr):6.0f}  "
          f"p5={np.percentile(arr, 5):5.0f}  p95={np.percentile(arr, 95):5.0f} ns")
    return np.median(arr)


def bench_tight_loop(fn, keys, label, iterations=5):
    """Time a tight loop of all keys, report per-key ns."""
    # Warmup
    for key in keys[:1000]:
        fn(key)

    best = float('inf')
    for _ in range(iterations):
        t0 = time.perf_counter_ns()
        for key in keys:
            fn(key)
        t1 = time.perf_counter_ns()
        per_key = (t1 - t0) / len(keys)
        best = min(best, per_key)
    print(f"  {label:30s}  best={best:6.1f} ns/query")
    return best


def main():
    keys = gen_keys(N, seed=SEED)
    rng = np.random.RandomState(SEED)
    indices = rng.choice(N, size=NUM_QUERY_KEYS, replace=False)
    query_keys = [keys[i] for i in indices]

    # Pre-encode keys as bytes to measure encoding overhead separately
    query_keys_bytes = [k.encode('utf-8') if isinstance(k, str) else k for k in query_keys]

    values = gen_alpha_values(N, 0.5, seed=SEED, minority_dist="uniform_100")
    config = BinaryFuseFilterConfig(fingerprint_bits=8)
    csf = carameldb.Caramel(keys, values, prefilter=config, verbose=False)

    csf_raw = csf

    print("=== Tight loop (best of 5 iterations) ===")
    print()

    # 1. Raw C++ (benchmark_queries)
    _, cpp_ns = csf_raw.benchmark_queries(query_keys, 10)
    print(f"  {'C++ internal loop':30s}  best={cpp_ns:6.1f} ns/query")

    # 2. Python for-loop overhead baseline (do nothing)
    bench_tight_loop(lambda k: None, query_keys, "Python no-op loop")

    # 3. str.encode() only
    bench_tight_loop(lambda k: k.encode('utf-8'), query_keys, "str.encode('utf-8') only")

    # 4. query() with str keys (current path)
    bench_tight_loop(csf_raw.query, query_keys, "query(str_key) [current]")

    # 5. query() with pre-encoded bytes keys
    bench_tight_loop(csf_raw.query, query_keys_bytes, "query(bytes_key)")

    print()
    print("=== Individual call timing (median) ===")
    print()

    bench_loop(csf_raw.query, query_keys, "query(str_key) [current]")
    bench_loop(csf_raw.query, query_keys_bytes, "query(bytes_key)")


if __name__ == "__main__":
    main()
