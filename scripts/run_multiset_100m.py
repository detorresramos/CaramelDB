"""N=100M, M=10, asin_clicks, 4 strategies. Tracks permute and build time separately."""

import json
import os
import sys
import tempfile
import time

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from benchmark_multiset import (
    MEASURE_TRIALS,
    NUM_QUERY_KEYS,
    SEED,
    WARMUP_TRIALS,
    column_entropy,
    load_empirical_tiers,
    make_permutation_config,
    measure_query_throughput,
    sample_empirical,
    verify_correctness,
)

import carameldb
from carameldb import AutoFilterConfig

CSV_PATH = "/Users/david/Downloads/caramel_asin_click_distribution.csv"
N = 100_000_000
M = 10

STRATEGIES = [
    ("Column", "none", False),
    ("Column+Permute", "global_sort", False),
    ("Column+SharedCB", "none", True),
    ("Column+SharedCB+Permute", "global_sort", True),
]

OUTPUT_JSON = "scripts/multiset_100m_results.json"


def main():
    tier_starts, tier_counts, tier_probs, total_unique = load_empirical_tiers(CSV_PATH)
    print(f"Dataset: asin_clicks, {total_unique:,} unique items")
    print(f"N={N:,}, M={M}")
    print("=" * 80)

    print("Generating keys...", flush=True)
    t0 = time.perf_counter()
    keys = [f"q{i}" for i in range(N)]
    print(f"  keys: {time.perf_counter() - t0:.1f}s", flush=True)

    print("Sampling values...", flush=True)
    t0 = time.perf_counter()
    rng = np.random.default_rng(SEED)
    values = sample_empirical(tier_starts, tier_counts, tier_probs, N, M, rng)
    print(f"  values: {time.perf_counter() - t0:.1f}s, shape={values.shape}, dtype={values.dtype}", flush=True)

    results = []

    for name, perm, shared_cb in STRATEGIES:
        print(f"\n--- {name} ---", flush=True)

        # Copy values so permutation doesn't affect next strategy
        vals = values.copy()

        # Permute
        perm_config = make_permutation_config(perm, {"refinement_iterations": 5})
        if perm_config is not None:
            print("  Permuting...", flush=True)
            t0 = time.perf_counter()
            carameldb.permute_uint32(vals, config=perm_config)
            perm_time = time.perf_counter() - t0
            print(f"  Permute: {perm_time:.2f}s", flush=True)
        else:
            perm_time = 0.0

        # Build
        print("  Building CSF...", flush=True)
        t0 = time.perf_counter()
        csf = carameldb.Caramel(
            keys, vals,
            permutation=None,
            prefilter=AutoFilterConfig(),
            shared_codebook=shared_cb,
            verbose=False,
        )
        build_time = time.perf_counter() - t0
        print(f"  Build: {build_time:.2f}s", flush=True)

        # Size
        with tempfile.NamedTemporaryFile(suffix=".csf", delete=False) as tmp:
            tmp_path = tmp.name
        try:
            csf.save(tmp_path)
            size_bytes = os.path.getsize(tmp_path)
        finally:
            os.unlink(tmp_path)
        bits_per_key = (size_bytes * 8) / N
        print(f"  Size: {size_bytes:,} bytes = {bits_per_key:.2f} bits/key", flush=True)

        # Query latency
        print("  Measuring query latency...", flush=True)
        query_ns = measure_query_throughput(csf, keys, WARMUP_TRIALS, MEASURE_TRIALS)
        print(f"  Query: {query_ns:.1f} ns/query", flush=True)

        # Correctness
        print("  Verifying correctness...", flush=True)
        verify_correctness(csf, keys, vals)
        print("  OK", flush=True)

        result = {
            "strategy": name,
            "N": N,
            "M": M,
            "vocab": total_unique,
            "permutation_s": round(perm_time, 2),
            "build_s": round(build_time, 2),
            "total_construction_s": round(perm_time + build_time, 2),
            "size_bytes": size_bytes,
            "bits_per_key": round(bits_per_key, 4),
            "query_ns": round(query_ns, 1),
        }
        results.append(result)
        print(f"  Summary: perm={perm_time:.2f}s  build={build_time:.2f}s  "
              f"total={perm_time+build_time:.2f}s  {bits_per_key:.2f} bits/key  {query_ns:.1f} ns/q", flush=True)

        # Save incrementally
        with open(OUTPUT_JSON, "w") as f:
            json.dump(results, f, indent=2)

        # Free CSF to reclaim memory before next strategy
        del csf

    print("\n" + "=" * 80)
    baseline = (M + 1) * 32
    print(f"uint32 baseline: {baseline} bits/key")
    print()
    print(f"{'Strategy':<30s} {'Perm(s)':>8s} {'Build(s)':>9s} {'Total(s)':>9s} {'bits/key':>10s} {'Query(ns)':>10s}")
    print("-" * 80)
    for r in results:
        print(f"{r['strategy']:<30s} {r['permutation_s']:>8.2f} {r['build_s']:>9.2f} "
              f"{r['total_construction_s']:>9.2f} {r['bits_per_key']:>10.2f} {r['query_ns']:>10.1f}")
    print(f"\nResults → {OUTPUT_JSON}")


if __name__ == "__main__":
    main()
