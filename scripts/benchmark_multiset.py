"""Multiset CSF benchmark: permutation strategies, filters, and codebook sharing.

Measures permutation time, construction time, serialized size (bits/key),
per-column entropy (before/after permutation), and query throughput.

Examples:
    # Single run with defaults (auto filter, no permutation)
    python scripts/benchmark_multiset.py

    # Vary distribution and permutation
    python scripts/benchmark_multiset.py --distribution zipfian --exponent 2.0 \
        --permutation entropy --num-rows 100000 --num-cols 50

    # Full sweep
    python scripts/benchmark_multiset.py --sweep

    # Custom sweep axes
    python scripts/benchmark_multiset.py --sweep \
        --sweep-num-rows 10000 100000 \
        --sweep-num-cols 10 50 100 \
        --sweep-vocab-size 1000 10000 \
        --sweep-exponents 0.5 1.0 1.5 2.0 \
        --sweep-permutations none entropy global_sort \
        --sweep-prefilters auto none \
        --sweep-shared-codebook true false
"""

import argparse
import itertools
import json
import os
import tempfile
import time

import numpy as np
from scipy.stats import entropy as scipy_entropy

import carameldb
from carameldb import (
    AutoFilterConfig,
    BinaryFuseFilterConfig,
    BloomFilterConfig,
    EntropyPermutationConfig,
    GlobalSortPermutationConfig,
    XORFilterConfig,
)

NUM_QUERY_KEYS = 250
WARMUP_TRIALS = 3
MEASURE_TRIALS = 10
SEED = 42


def gen_multiset_data(num_rows, num_cols, vocab_size, distribution, dist_params, seed):
    """
    Returns (keys: list[str], values: np.ndarray of shape (num_rows, num_cols)).

    Each row samples num_cols items WITHOUT replacement from a vocabulary of
    vocab_size items, weighted by the chosen distribution.
    """
    rng = np.random.default_rng(seed)

    if distribution == "uniform":
        probs = np.ones(vocab_size) / vocab_size
    elif distribution == "zipfian":
        exponent = dist_params.get("exponent", 1.5)
        ranks = np.arange(1, vocab_size + 1, dtype=np.float64)
        probs = 1.0 / ranks ** exponent
        probs /= probs.sum()
    elif distribution == "alpha":
        alpha = dist_params.get("alpha", 0.9)
        minority_dist = dist_params.get("minority_dist", "zipfian")
        if minority_dist == "uniform":
            minority_probs = np.ones(vocab_size - 1)
        elif minority_dist == "zipfian":
            ranks = np.arange(1, vocab_size, dtype=np.float64)
            minority_probs = 1.0 / ranks ** dist_params.get("exponent", 1.5)
        else:
            raise ValueError(f"Unknown minority distribution: {minority_dist}")
        minority_probs = minority_probs / minority_probs.sum() * (1 - alpha)
        probs = np.empty(vocab_size)
        probs[0] = alpha
        probs[1:] = minority_probs
    else:
        raise ValueError(f"Unknown distribution: {distribution}")

    values = np.empty((num_rows, num_cols), dtype=np.uint32)
    for i in range(num_rows):
        values[i] = rng.choice(vocab_size, size=num_cols, replace=False, p=probs)

    keys = [f"query_{i}" for i in range(num_rows)]
    return keys, values


def column_entropy(values):
    """Compute per-column empirical entropy (bits) for a 2D array."""
    _, num_cols = values.shape
    entropies = np.empty(num_cols)
    for c in range(num_cols):
        _, counts = np.unique(values[:, c], return_counts=True)
        entropies[c] = scipy_entropy(counts, base=2)
    return entropies


def apply_permutation_inplace(values, permutation, permutation_params):
    """Apply permutation in-place and return the time taken."""
    if permutation == "none":
        return 0.0

    perm_config = make_permutation_config(permutation, permutation_params)
    t0 = time.perf_counter()
    carameldb.permute_uint32(values, config=perm_config)
    perm_time = time.perf_counter() - t0
    return perm_time


def make_permutation_config(permutation, permutation_params):
    if permutation == "none":
        return None
    elif permutation == "entropy":
        return EntropyPermutationConfig()
    elif permutation == "global_sort":
        iters = permutation_params.get("refinement_iterations", 5)
        return GlobalSortPermutationConfig(refinement_iterations=iters)
    else:
        raise ValueError(f"Unknown permutation: {permutation}")


def make_prefilter_config(prefilter, filter_params):
    if prefilter is None or prefilter == "none":
        return None
    elif prefilter == "auto":
        return AutoFilterConfig()
    elif prefilter == "binary_fuse":
        return BinaryFuseFilterConfig(fingerprint_bits=filter_params.get("fingerprint_bits", 8))
    elif prefilter == "bloom":
        return BloomFilterConfig(
            bits_per_element=filter_params.get("bits_per_element", 10),
            num_hashes=filter_params.get("num_hashes", 7),
        )
    elif prefilter == "xor":
        return XORFilterConfig(fingerprint_bits=filter_params.get("fingerprint_bits", 8))
    else:
        raise ValueError(f"Unknown prefilter: {prefilter}")


def measure_query_throughput(csf, keys, warmup, measured):
    n_sample = min(NUM_QUERY_KEYS, len(keys))
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
    return float(np.median(trial_ns))


def verify_correctness(csf, keys, values, num_checks=50):
    """Spot-check that queries return correct results."""
    rng = np.random.RandomState(SEED + 1)
    n_check = min(num_checks, len(keys))
    indices = rng.choice(len(keys), size=n_check, replace=False)
    for idx in indices:
        result = csf.query(keys[idx])
        expected = sorted(values[idx].tolist())
        actual = sorted(result)
        if actual != expected:
            raise ValueError(
                f"Correctness check failed for key {keys[idx]}: "
                f"expected {expected}, got {actual}"
            )


def run_single(config, verbose=True):
    keys, values = gen_multiset_data(
        config["num_rows"], config["num_cols"], config["vocab_size"],
        config["distribution"], config["dist_params"], SEED,
    )

    # Per-column entropy before permutation
    entropy_before = column_entropy(values)

    # Permute in-place, measuring time separately
    perm_time = apply_permutation_inplace(
        values, config["permutation"], config.get("permutation_params", {})
    )
    entropy_after = column_entropy(values)

    # Build CSF on already-permuted data (no permutation config — already done)
    prefilter_config = make_prefilter_config(config.get("prefilter", "auto"), config.get("filter_params", {}))

    label = (
        f"N={config['num_rows']:,}, M={config['num_cols']}, "
        f"|Σ|={config['vocab_size']:,}, {config['distribution']}"
    )
    if config["distribution"] == "zipfian":
        label += f"(s={config['dist_params'].get('exponent', 1.5)})"
    elif config["distribution"] == "alpha":
        label += f"(α={config['dist_params'].get('alpha', 0.9)})"
    label += f", perm={config['permutation']}, filter={config.get('prefilter', 'auto')}"
    if config.get("shared_codebook"):
        label += ", shared_cb"
    if config.get("shared_filter"):
        label += ", shared_filt"

    if verbose:
        print(f"  {label}...", end="", flush=True)

    # Construction (permutation already applied, just filter + CSF building)
    t0 = time.perf_counter()
    csf = carameldb.Caramel(
        keys, values,
        permutation=None,
        prefilter=prefilter_config,
        shared_codebook=config.get("shared_codebook", False),
        shared_filter=config.get("shared_filter", False),
        verbose=False,
    )
    build_s = time.perf_counter() - t0

    # Correctness check
    verify_correctness(csf, keys, values)

    # Serialized size
    with tempfile.NamedTemporaryFile(suffix=".csf", delete=False) as tmp:
        tmp_path = tmp.name
    try:
        csf.save(tmp_path)
        size_bytes = os.path.getsize(tmp_path)
    finally:
        os.unlink(tmp_path)

    bits_per_key = (size_bytes * 8) / config["num_rows"]

    # Query throughput
    query_ns = measure_query_throughput(csf, keys, WARMUP_TRIALS, MEASURE_TRIALS)

    if verbose:
        print(
            f"\n    Perm: {perm_time:.2f}s | Build: {build_s:.2f}s | "
            f"{bits_per_key:.2f} bits/key | Query: {query_ns:.1f} ns/query"
        )
        print(
            f"    Entropy before: {entropy_before.sum():.2f} total, "
            f"[{', '.join(f'{e:.2f}' for e in entropy_before[:5])}{'...' if len(entropy_before) > 5 else ''}]"
        )
        print(
            f"    Entropy after:  {entropy_after.sum():.2f} total, "
            f"[{', '.join(f'{e:.2f}' for e in entropy_after[:5])}{'...' if len(entropy_after) > 5 else ''}]"
        )

    return {
        "num_rows": config["num_rows"],
        "num_cols": config["num_cols"],
        "vocab_size": config["vocab_size"],
        "distribution": config["distribution"],
        "dist_params": config["dist_params"],
        "permutation": config["permutation"],
        "permutation_params": config.get("permutation_params", {}),
        "prefilter": config.get("prefilter", "auto"),
        "filter_params": config.get("filter_params", {}),
        "shared_codebook": config.get("shared_codebook", False),
        "shared_filter": config.get("shared_filter", False),
        "permutation_s": round(perm_time, 4),
        "build_s": round(build_s, 4),
        "total_construction_s": round(perm_time + build_s, 4),
        "size_bytes": size_bytes,
        "bits_per_key": round(bits_per_key, 4),
        "query_ns": round(query_ns, 2),
        "entropy_before_total": round(float(entropy_before.sum()), 4),
        "entropy_after_total": round(float(entropy_after.sum()), 4),
        "entropy_before_per_col": [round(float(e), 4) for e in entropy_before],
        "entropy_after_per_col": [round(float(e), 4) for e in entropy_after],
    }


# Default sweep configurations
DEFAULT_SWEEP_NUM_ROWS = [10_000, 100_000]
DEFAULT_SWEEP_NUM_COLS = [10, 50, 100]
DEFAULT_SWEEP_VOCAB_SIZES = [1_000, 10_000]
DEFAULT_SWEEP_EXPONENTS = [0.5, 1.0, 1.5, 2.0]
DEFAULT_SWEEP_PERMUTATIONS = ["none", "entropy", "global_sort"]
DEFAULT_SWEEP_PREFILTERS = ["auto", "none"]
DEFAULT_SWEEP_SHARED_CODEBOOK = [False]


def build_sweep_configs(
    num_rows_list, num_cols_list, vocab_size_list, exponents,
    permutations, prefilters, shared_codebook_list,
    filter_params, shared_filter,
):
    configs = []
    for num_rows, num_cols, vocab_size, exponent in itertools.product(
        num_rows_list, num_cols_list, vocab_size_list, exponents
    ):
        if num_cols >= vocab_size:
            continue

        for perm, prefilter, shared_cb in itertools.product(
            permutations, prefilters, shared_codebook_list
        ):
            configs.append({
                "num_rows": num_rows,
                "num_cols": num_cols,
                "vocab_size": vocab_size,
                "distribution": "zipfian",
                "dist_params": {"exponent": exponent},
                "permutation": perm,
                "permutation_params": {},
                "prefilter": prefilter,
                "filter_params": filter_params,
                "shared_codebook": shared_cb,
                "shared_filter": shared_filter,
            })
    return configs


def main():
    parser = argparse.ArgumentParser(
        description="CaramelDB multiset CSF benchmark",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )

    # Single-run parameters
    single = parser.add_argument_group("single run parameters")
    single.add_argument("--num-rows", type=int, default=10_000, help="Number of keys (N)")
    single.add_argument("--num-cols", type=int, default=10, help="Values per key (M)")
    single.add_argument("--vocab-size", type=int, default=1_000, help="Vocabulary size (|Σ|)")
    single.add_argument("--distribution", type=str, default="zipfian",
                        choices=["zipfian", "uniform", "alpha"])
    single.add_argument("--exponent", type=float, default=1.5, help="Zipfian exponent (s)")
    single.add_argument("--alpha", type=float, default=0.9, help="Dominant value fraction (alpha dist)")
    single.add_argument("--permutation", type=str, default="none",
                        choices=["none", "entropy", "global_sort"])
    single.add_argument("--refinement-iterations", type=int, default=5,
                        help="Refinement iterations for global_sort permutation")
    single.add_argument("--prefilter", type=str, default="auto",
                        choices=["auto", "none", "binary_fuse", "bloom", "xor"])
    single.add_argument("--fingerprint-bits", type=int, default=8)
    single.add_argument("--shared-codebook", action="store_true")
    single.add_argument("--shared-filter", action="store_true")

    # Sweep parameters
    sweep = parser.add_argument_group("sweep parameters")
    sweep.add_argument("--sweep", action="store_true", help="Run parameter sweep")
    sweep.add_argument("--sweep-num-rows", type=int, nargs="+", default=None,
                       help=f"N values to sweep (default: {DEFAULT_SWEEP_NUM_ROWS})")
    sweep.add_argument("--sweep-num-cols", type=int, nargs="+", default=None,
                       help=f"M values to sweep (default: {DEFAULT_SWEEP_NUM_COLS})")
    sweep.add_argument("--sweep-vocab-size", type=int, nargs="+", default=None,
                       help=f"|Σ| values to sweep (default: {DEFAULT_SWEEP_VOCAB_SIZES})")
    sweep.add_argument("--sweep-exponents", type=float, nargs="+", default=None,
                       help=f"Zipfian exponents to sweep (default: {DEFAULT_SWEEP_EXPONENTS})")
    sweep.add_argument("--sweep-permutations", type=str, nargs="+", default=None,
                       choices=["none", "entropy", "global_sort"],
                       help=f"Permutations to sweep (default: {DEFAULT_SWEEP_PERMUTATIONS})")
    sweep.add_argument("--sweep-prefilters", type=str, nargs="+", default=None,
                       choices=["auto", "none", "binary_fuse", "bloom", "xor"],
                       help=f"Prefilters to sweep (default: {DEFAULT_SWEEP_PREFILTERS})")
    sweep.add_argument("--sweep-shared-codebook", type=str, nargs="+", default=None,
                       help=f"Shared codebook values (default: {DEFAULT_SWEEP_SHARED_CODEBOOK})")

    # Output
    parser.add_argument("--output", type=str, default=None, help="Output JSON path")

    args = parser.parse_args()

    if args.distribution == "alpha":
        dist_params = {"alpha": args.alpha, "minority_dist": "zipfian"}
    elif args.distribution == "zipfian":
        dist_params = {"exponent": args.exponent}
    else:
        dist_params = {}

    filter_params = {"fingerprint_bits": args.fingerprint_bits}
    perm_params = {}
    if args.permutation == "global_sort":
        perm_params["refinement_iterations"] = args.refinement_iterations

    if args.sweep:
        num_rows_list = args.sweep_num_rows or DEFAULT_SWEEP_NUM_ROWS
        num_cols_list = args.sweep_num_cols or DEFAULT_SWEEP_NUM_COLS
        vocab_size_list = args.sweep_vocab_size or DEFAULT_SWEEP_VOCAB_SIZES
        exponents = args.sweep_exponents or DEFAULT_SWEEP_EXPONENTS
        permutations = args.sweep_permutations or DEFAULT_SWEEP_PERMUTATIONS
        prefilters = args.sweep_prefilters or DEFAULT_SWEEP_PREFILTERS

        if args.sweep_shared_codebook is not None:
            shared_cb_list = [v.lower() in ("true", "1", "yes") for v in args.sweep_shared_codebook]
        else:
            shared_cb_list = DEFAULT_SWEEP_SHARED_CODEBOOK

        configs = build_sweep_configs(
            num_rows_list, num_cols_list, vocab_size_list, exponents,
            permutations, prefilters, shared_cb_list,
            filter_params, args.shared_filter,
        )
    else:
        configs = [{
            "num_rows": args.num_rows,
            "num_cols": args.num_cols,
            "vocab_size": args.vocab_size,
            "distribution": args.distribution,
            "dist_params": dist_params,
            "permutation": args.permutation,
            "permutation_params": perm_params,
            "prefilter": args.prefilter,
            "filter_params": filter_params,
            "shared_codebook": args.shared_codebook,
            "shared_filter": args.shared_filter,
        }]

    total = len(configs)
    print(f"CaramelDB Multiset Benchmark ({total} configs)")
    print("=" * 80)

    results = []
    t_start = time.perf_counter()
    for i, cfg in enumerate(configs):
        elapsed = time.perf_counter() - t_start
        if i > 0:
            eta = elapsed / i * (total - i)
            print(f"\n[{i + 1}/{total}] (ETA: {eta:.0f}s)", end="")
        else:
            print(f"\n[{i + 1}/{total}]", end="")
        result = run_single(cfg)
        results.append(result)

    # Summary table
    print("\n" + "=" * 80)
    header = (
        f"{'N':>8} {'M':>4} {'|Σ|':>6} {'dist':<12} "
        f"{'perm':<12} {'filter':<6} {'cb':<4} "
        f"{'perm(s)':>7} {'build(s)':>8} {'total(s)':>8} {'bits/key':>8} {'query(ns)':>9} "
        f"{'H_before':>8} {'H_after':>8}"
    )
    print(f"\nSummary:\n{header}")
    print("-" * len(header))
    for r in results:
        dist_label = r["distribution"]
        if r["distribution"] == "zipfian":
            dist_label += f"({r['dist_params'].get('exponent', 1.5)})"
        elif r["distribution"] == "alpha":
            dist_label += f"({r['dist_params'].get('alpha', 0.9)})"
        print(
            f"{r['num_rows']:>8} {r['num_cols']:>4} {r['vocab_size']:>6} {dist_label:<12} "
            f"{r['permutation']:<12} {r['prefilter']:<6} "
            f"{'yes' if r['shared_codebook'] else 'no':<4} "
            f"{r['permutation_s']:>7.2f} {r['build_s']:>8.2f} {r['total_construction_s']:>8.2f} "
            f"{r['bits_per_key']:>8.2f} {r['query_ns']:>9.1f} "
            f"{r['entropy_before_total']:>8.2f} {r['entropy_after_total']:>8.2f}"
        )

    if args.output:
        out_path = args.output
    else:
        out_path = os.path.join(os.path.dirname(__file__), "benchmark_multiset_results.json")

    with open(out_path, "w") as f:
        json.dump(results, f, indent=2)
    print(f"\nResults saved to {out_path}")


if __name__ == "__main__":
    main()
