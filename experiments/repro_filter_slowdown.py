"""
A/B benchmark: compare current code against the baseline (bucket_size=1000).

Builds and benchmarks both versions across all (alpha, distribution, filter)
combinations. Exits 1 if any combination is slower than baseline.
"""

import json
import os
import subprocess
import sys
import time

import numpy as np


N = 1_000_000
TOLERANCE = 1.10  # allow 10% noise


def generate_values(n, alpha, distribution):
    num_common = int(n * alpha)
    num_minority = n - num_common
    COMMON_VALUE = 999999

    if distribution == "unique":
        minority_values = list(range(num_minority))
    elif distribution == "zipf":
        num_distinct = max(10, int(num_minority ** 0.5))
        rng = np.random.default_rng(42)
        minority_values = (rng.zipf(1.5, size=num_minority) % num_distinct).tolist()
    elif distribution == "uniform-10":
        minority_values = [i % 10 for i in range(num_minority)]
    else:
        raise ValueError(f"Unknown distribution: {distribution}")

    return [COMMON_VALUE] * num_common + minority_values


def time_build(keys, values, prefilter):
    import carameldb
    t0 = time.time()
    carameldb.Caramel(keys, values, prefilter=prefilter, verbose=False)
    return time.time() - t0


def get_filters():
    from carameldb import XORFilterConfig, BloomFilterConfig, BinaryFuseFilterConfig
    return {
        "none": None,
        "xor-8": XORFilterConfig(fingerprint_bits=8),
        "bloom": BloomFilterConfig(bits_per_element=10, num_hashes=7),
        "bfuse-8": BinaryFuseFilterConfig(fingerprint_bits=8),
    }


CONFIGS = [
    (alpha, dist)
    for alpha in [0.1, 0.2, 0.3, 0.5, 0.8, 0.9, 0.95, 0.99]
    for dist in ["unique", "zipf", "uniform-10"]
]


def run_all(num_runs=3):
    keys = [f"key{i}" for i in range(N)]
    filters = get_filters()
    results = {}
    for alpha, dist in CONFIGS:
        values = generate_values(N, alpha, dist)
        for fname, fconfig in filters.items():
            key = f"{alpha}|{dist}|{fname}"
            best = min(time_build(keys, values, fconfig) for _ in range(num_runs))
            results[key] = best
    return results


def print_table(baseline, current):
    filters = ["none", "xor-8", "bloom", "bfuse-8"]
    header = f"{'alpha':>5} {'dist':<12} " + " ".join(f"{'Δ '+f:>10}" for f in filters)
    print(header)
    print("-" * len(header))

    failures = []
    for alpha, dist in CONFIGS:
        cols = []
        for f in filters:
            key = f"{alpha}|{dist}|{f}"
            b = baseline[key]
            c = current[key]
            ratio = c / b
            marker = " !" if ratio > TOLERANCE else ""
            if ratio > TOLERANCE:
                failures.append((alpha, dist, f, b, c, ratio))
            cols.append(f"{ratio:>9.2f}x" + marker)
        print(f"{alpha:>5} {dist:<12} " + " ".join(cols))

    return failures


if __name__ == "__main__":
    baseline_file = "/tmp/caramel_bucket_baseline.json"

    if "--baseline" in sys.argv:
        print(f"Running baseline (n={N:,})...")
        results = run_all()
        with open(baseline_file, "w") as f:
            json.dump(results, f)
        print(f"Saved to {baseline_file}")
        sys.exit(0)

    if not os.path.exists(baseline_file):
        print(f"No baseline found at {baseline_file}")
        print("Run with --baseline first (on the old code).")
        sys.exit(1)

    with open(baseline_file) as f:
        baseline = json.load(f)

    print(f"Running current code (n={N:,})...")
    current = run_all()

    print(f"\nRatio = current / baseline (< 1.0 = faster)")
    print()
    failures = print_table(baseline, current)

    print()
    if failures:
        print("REGRESSIONS (current slower than baseline):")
        for alpha, dist, f, b, c, ratio in failures:
            print(f"  alpha={alpha}, dist={dist}, filter={f}: "
                  f"{b:.3f}s -> {c:.3f}s ({ratio:.2f}x)")
        sys.exit(1)
    else:
        print("PASS: current code is as fast or faster than baseline everywhere.")
        sys.exit(0)
