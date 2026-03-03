"""Measure Python binding overhead for CaramelDB queries."""

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


def main():
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
        print(f"\n--- {label} ---")
        values = gen_alpha_values(N, alpha, seed=SEED, minority_dist=dist)
        config = BinaryFuseFilterConfig(fingerprint_bits=8)
        csf = carameldb.Caramel(keys, values, prefilter=config, verbose=False)

        if isinstance(csf, carameldb.CSFQueryWrapper):
            csf_raw = csf._csf
        else:
            csf_raw = csf

        # Warmup
        for key in query_keys[:1000]:
            csf_raw.query(key)

        # Measure each call individually
        times_ns = []
        for key in query_keys:
            t0 = time.perf_counter_ns()
            csf_raw.query(key)
            t1 = time.perf_counter_ns()
            times_ns.append(t1 - t0)

        times_ns = np.array(times_ns)

        # C++ baseline
        _, cpp_ns = csf_raw.benchmark_queries(query_keys, 5)

        print(f"  C++ loop:        {cpp_ns:7.1f} ns/query")
        print(f"  Python calls:    median={np.median(times_ns):.0f}  "
              f"mean={np.mean(times_ns):.0f}  "
              f"p5={np.percentile(times_ns, 5):.0f}  "
              f"p95={np.percentile(times_ns, 95):.0f} ns/query")
        print(f"  Overhead:        ~{np.median(times_ns) - cpp_ns:.0f} ns/query")


if __name__ == "__main__":
    main()
