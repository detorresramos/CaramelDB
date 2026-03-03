"""Cython migration benchmark: construction time, serialized size, and query throughput.

Compares before (pybind11) vs after (Cython) by measuring:
- Construction time (seconds)
- Serialized size (bytes, bits/key)
- Query throughput (ns/query) via benchmark_queries() or Python fallback
"""

import argparse
import json
import os
import random
import sys
import tempfile
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import carameldb
from carameldb import BinaryFuseFilterConfig
from data_gen import gen_alpha_values, gen_keys

NUM_QUERY_KEYS = 250
WARMUP_TRIALS = 3
MEASURE_TRIALS = 10
SEED = 42

SWEEP_CONFIGS = []
for n in [100_000, 10_000_000]:
    for alpha in [0.2, 0.5, 0.9]:
        for dist in ["zipfian", "uniform_100", "unique"]:
            SWEEP_CONFIGS.append({"n": n, "alpha": alpha, "dist": dist})

# Single large run
SWEEP_CONFIGS.append({"n": 100_000_000, "alpha": 0.5, "dist": "zipfian"})


FIXED_KEY_REPEATS = 1000


def _time_fixed_key(csf, key, warmup, measured):
    """Time a single key repeated many times. Returns median ns/query."""
    csf.query(key)
    trial_ns = []
    for t in range(warmup + measured):
        start = time.perf_counter()
        for _ in range(FIXED_KEY_REPEATS):
            csf.query(key)
        elapsed = time.perf_counter() - start
        if t >= warmup:
            trial_ns.append((elapsed / FIXED_KEY_REPEATS) * 1e9)
    return float(np.median(trial_ns))


def measure_query_throughput(csf, keys, values, num_query_keys, warmup, measured):
    """Measure query throughput: random keys, fixed majority key, fixed minority key."""
    n_sample = min(num_query_keys, len(keys))

    # Random keys (different each trial = cold cache, realistic workload)
    rng = np.random.RandomState(SEED)
    trial_ns = []
    for t in range(warmup + measured):
        indices = rng.choice(len(keys), size=n_sample, replace=False)
        query_keys = [keys[i] for i in indices]
        start = time.perf_counter()
        for k in query_keys:
            csf.query(k)
        elapsed = time.perf_counter() - start
        if t >= warmup:
            trial_ns.append((elapsed / n_sample) * 1e9)
    random_ns = float(np.median(trial_ns))

    # Fixed majority key (filter short-circuits, no CSF lookup)
    majority_value = np.uint32(2**32 - 1)
    majority_indices = np.where(values == majority_value)[0]
    majority_key = keys[majority_indices[0]]
    majority_ns = _time_fixed_key(csf, majority_key, warmup, measured)

    # Fixed minority key (full CSF lookup path)
    minority_indices = np.where(values != majority_value)[0]
    minority_key = keys[minority_indices[0]]
    minority_ns = _time_fixed_key(csf, minority_key, warmup, measured)

    return random_ns, majority_ns, minority_ns


def run_single(n, alpha, dist, verbose=True):
    """Run a single benchmark configuration. Returns a result dict."""
    if verbose:
        print(f"\n  N={n:,}, alpha={alpha}, dist={dist}")

    keys = gen_keys(n, seed=SEED)
    values = gen_alpha_values(n, alpha, seed=SEED, minority_dist=dist)
    config = BinaryFuseFilterConfig(fingerprint_bits=8)

    # Construction time
    if verbose:
        print("    Building...", end="", flush=True)
    t0 = time.perf_counter()
    csf = carameldb.Caramel(keys, values, prefilter=config, verbose=False)
    construction_s = time.perf_counter() - t0
    if verbose:
        print(f" {construction_s:.2f}s")

    # Serialized size
    with tempfile.NamedTemporaryFile(suffix=".csf", delete=False) as tmp:
        tmp_path = tmp.name
    try:
        csf.save(tmp_path)
        size_bytes = os.path.getsize(tmp_path)
    finally:
        os.unlink(tmp_path)

    bits_per_key = (size_bytes * 8) / n

    # Query throughput
    if verbose:
        print("    Querying...", end="", flush=True)
    random_ns, majority_ns, minority_ns = measure_query_throughput(
        csf, keys, values, NUM_QUERY_KEYS, WARMUP_TRIALS, MEASURE_TRIALS
    )
    if verbose:
        print(f" random={random_ns:.1f}, majority={majority_ns:.1f}, minority={minority_ns:.1f} ns/query")

    result = {
        "n": n,
        "alpha": alpha,
        "dist": dist,
        "construction_s": round(construction_s, 3),
        "size_bytes": size_bytes,
        "bits_per_key": round(bits_per_key, 3),
        "random_ns": round(random_ns, 2),
        "majority_ns": round(majority_ns, 2),
        "minority_ns": round(minority_ns, 2),
    }

    if verbose:
        print(f"    Size: {size_bytes:,} bytes ({bits_per_key:.3f} bits/key)")

    return result


def _results_table(results, label):
    """Generate a standalone results table."""
    lines = [
        f"## {label}",
        "",
        "| N | Alpha | Distribution | Construction (s) | Random (ns/q) | Majority (ns/q) | Minority (ns/q) | Size (bits/key) |",
        "|--:|------:|:-------------|------------------:|--------------:|----------------:|----------------:|----------------:|",
    ]
    for r in results:
        lines.append(
            f"| {r['n']:,} | {r['alpha']} | {r['dist']} "
            f"| {r['construction_s']:.2f} | {r['random_ns']:.1f} "
            f"| {r['majority_ns']:.1f} | {r['minority_ns']:.1f} "
            f"| {r['bits_per_key']:.3f} |"
        )
    lines.append("")
    return lines


def generate_table(before_path, after_path, output_path):
    """Generate markdown tables from before/after JSON results."""
    with open(before_path) as f:
        before_results = json.load(f)
    with open(after_path) as f:
        after_results = json.load(f)

    before_map = {(r["n"], r["alpha"], r["dist"]): r for r in before_results}
    after_map = {(r["n"], r["alpha"], r["dist"]): r for r in after_results}

    all_keys = list(dict.fromkeys(
        [(r["n"], r["alpha"], r["dist"]) for r in after_results]
        + [(r["n"], r["alpha"], r["dist"]) for r in before_results]
    ))

    lines = [
        "# Cython Migration Benchmark Results",
        "",
        "Comparison of before (`121aa3f`) vs after (`c3586d8`). Changes include:",
        "",
        "1. **Python bindings**: pybind11 → Cython (reduces per-call binding overhead)",
        "2. **Huffman lookup table**: replaced sequential `canonicalDecodeFromNumber` with O(1) table lookup",
        "3. **Query cache**: pre-computed `BucketQueryInfo` avoids per-query indirection through solution vectors",
        "",
        "All inference measured via Python `query()` loop (includes binding overhead).",
        "",
        "- **Random**: different random keys each trial (cold cache, realistic workload)",
        "- **Majority**: single majority-value key repeated (filter short-circuit path, hot cache)",
        "- **Minority**: single minority-value key repeated (full CSF lookup path, hot cache)",
        "",
        f"Query measurement: {NUM_QUERY_KEYS:,} random keys/trial, "
        f"{WARMUP_TRIALS} warmup + {MEASURE_TRIALS} measured trials, median ns/query.",
        "",
    ]

    # Before table
    lines += _results_table(before_results, "Before (pybind11)")

    # After table
    lines += _results_table(after_results, "After (Cython)")

    # Comparison table
    lines += [
        "## Comparison",
        "",
        "| N | Alpha | Distribution | Construction Speedup | Random Speedup | Majority Speedup | Minority Speedup |",
        "|--:|------:|:-------------|---------------------:|---------------:|-----------------:|-----------------:|",
    ]

    def _speedup(before_val, after_val):
        return before_val / after_val if after_val > 0 else float("inf")

    for key in all_keys:
        b = before_map.get(key)
        a = after_map.get(key)
        if not b or not a:
            continue

        n, alpha, dist = key

        lines.append(
            f"| {n:,} | {alpha} | {dist} "
            f"| {_speedup(b['construction_s'], a['construction_s']):.2f}x "
            f"| {_speedup(b['random_ns'], a['random_ns']):.2f}x "
            f"| {_speedup(b['majority_ns'], a['majority_ns']):.2f}x "
            f"| {_speedup(b['minority_ns'], a['minority_ns']):.2f}x |"
        )

    lines.append("")

    with open(output_path, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"\nTable written to {output_path}")


def main():
    parser = argparse.ArgumentParser(description="CaramelDB Cython migration benchmark")
    parser.add_argument("--n", type=int, default=100_000, help="Number of keys")
    parser.add_argument("--alpha", type=float, default=0.5, help="Alpha (majority fraction)")
    parser.add_argument("--dist", type=str, default="zipfian", help="Minority distribution")
    parser.add_argument("--sweep", action="store_true", help="Run all sweep configs")
    parser.add_argument("--output", type=str, default=None, help="Output JSON path")
    parser.add_argument(
        "--table", nargs=2, metavar=("BEFORE", "AFTER"),
        help="Generate table from two JSON result files"
    )
    args = parser.parse_args()

    if args.table:
        out = args.output or os.path.join(os.path.dirname(__file__), "tables.md")
        generate_table(args.table[0], args.table[1], out)
        return

    if args.sweep:
        configs = SWEEP_CONFIGS
    else:
        configs = [{"n": args.n, "alpha": args.alpha, "dist": args.dist}]

    print(f"CaramelDB Benchmark ({len(configs)} configs)")
    print("=" * 60)

    results = []
    for i, cfg in enumerate(configs):
        print(f"\n[{i + 1}/{len(configs)}]", end="")
        result = run_single(**cfg)
        results.append(result)

    # Summary
    print("\n" + "=" * 60)
    print(f"\n{'N':>12} {'alpha':>6} {'dist':<14} {'build(s)':>9} {'random':>8} {'majority':>9} {'minority':>9} {'bits/key':>9}")
    print("-" * 85)
    for r in results:
        print(
            f"{r['n']:>12,} {r['alpha']:>6} {r['dist']:<14} "
            f"{r['construction_s']:>9.2f} {r['random_ns']:>8.1f} "
            f"{r['majority_ns']:>9.1f} {r['minority_ns']:>9.1f} "
            f"{r['bits_per_key']:>9.3f}"
        )

    if args.output:
        out_path = args.output
    else:
        out_path = os.path.join(os.path.dirname(__file__), "benchmark_results.json")

    with open(out_path, "w") as f:
        json.dump(results, f, indent=2)
    print(f"\nResults saved to {out_path}")


if __name__ == "__main__":
    main()
