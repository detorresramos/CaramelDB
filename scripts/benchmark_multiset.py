"""Multiset CSF benchmark: permutation strategies, filters, and codebook sharing.

Measures construction time, serialized size, and query throughput for the
multiset CSF pipeline across configurable data distributions and pipeline options.
"""

import argparse
import itertools
import json
import os
import tempfile
import time

import numpy as np

import carameldb
from carameldb import (
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
    if prefilter is None:
        return None
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


def run_single(config, verbose=True):
    keys, values = gen_multiset_data(
        config["num_rows"], config["num_cols"], config["vocab_size"],
        config["distribution"], config["dist_params"], SEED,
    )

    perm_config = make_permutation_config(config["permutation"], config.get("permutation_params", {}))
    prefilter_config = make_prefilter_config(config.get("prefilter"), config.get("filter_params", {}))

    label = (
        f"N={config['num_rows']:,}, cols={config['num_cols']}, "
        f"{config['distribution']}, {config['permutation']}"
    )
    if config.get("prefilter"):
        label += f", filter={config['prefilter']}"
    if config.get("shared_codebook"):
        label += ", shared_cb"
    if config.get("shared_filter"):
        label += ", shared_filt"

    if verbose:
        print(f"  {label}...", end="", flush=True)

    # Construction
    t0 = time.perf_counter()
    csf = carameldb.Caramel(
        keys, values,
        permutation=perm_config,
        prefilter=prefilter_config,
        shared_codebook=config.get("shared_codebook", False),
        shared_filter=config.get("shared_filter", False),
        verbose=False,
    )
    construction_s = time.perf_counter() - t0

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
        print(f"\n    Build: {construction_s:.2f}s | Size: {size_bytes:,} bytes ({bits_per_key:.1f} bits/key) | Query: {query_ns:.1f} ns/query")

    return {
        "num_rows": config["num_rows"],
        "num_cols": config["num_cols"],
        "vocab_size": config["vocab_size"],
        "distribution": config["distribution"],
        "dist_params": config["dist_params"],
        "permutation": config["permutation"],
        "permutation_params": config.get("permutation_params", {}),
        "prefilter": config.get("prefilter"),
        "filter_params": config.get("filter_params", {}),
        "shared_codebook": config.get("shared_codebook", False),
        "shared_filter": config.get("shared_filter", False),
        "construction_s": round(construction_s, 3),
        "size_bytes": size_bytes,
        "bits_per_key": round(bits_per_key, 3),
        "query_ns": round(query_ns, 2),
    }


DATA_CONFIGS = [
    {"vocab_size": 1000, "distribution": "zipfian", "dist_params": {"exponent": 1.5}},
    {"vocab_size": 1000, "distribution": "uniform", "dist_params": {}},
    {"vocab_size": 1000, "distribution": "alpha", "dist_params": {"alpha": 0.9, "minority_dist": "zipfian"}},
]
SIZES = [
    {"num_rows": 10_000, "num_cols": 10},
    {"num_rows": 10_000, "num_cols": 50},
    {"num_rows": 100_000, "num_cols": 10},
]
PERMUTATIONS = ["none", "entropy", "global_sort"]


def build_sweep_configs(prefilter, filter_params, shared_codebook, shared_filter):
    configs = []
    for data, size, perm in itertools.product(DATA_CONFIGS, SIZES, PERMUTATIONS):
        cfg = {**data, **size, "permutation": perm, "permutation_params": {}}
        if prefilter:
            cfg["prefilter"] = prefilter
            cfg["filter_params"] = filter_params
        if shared_codebook:
            cfg["shared_codebook"] = True
        if shared_filter:
            cfg["shared_filter"] = True
        configs.append(cfg)
    return configs


def main():
    parser = argparse.ArgumentParser(description="CaramelDB multiset CSF benchmark")
    parser.add_argument("--num-rows", type=int, default=1000)
    parser.add_argument("--num-cols", type=int, default=5)
    parser.add_argument("--vocab-size", type=int, default=1000)
    parser.add_argument("--distribution", type=str, default="zipfian",
                        choices=["zipfian", "uniform", "alpha"])
    parser.add_argument("--exponent", type=float, default=1.5)
    parser.add_argument("--alpha", type=float, default=0.9)
    parser.add_argument("--permutation", type=str, default="none",
                        choices=["none", "entropy", "global_sort"])
    parser.add_argument("--prefilter", type=str, default=None,
                        choices=["binary_fuse", "bloom", "xor"])
    parser.add_argument("--fingerprint-bits", type=int, default=8)
    parser.add_argument("--shared-codebook", action="store_true")
    parser.add_argument("--shared-filter", action="store_true")
    parser.add_argument("--sweep", action="store_true", help="Run full sweep")
    parser.add_argument("--output", type=str, default=None, help="Output JSON path")
    args = parser.parse_args()

    if args.distribution == "alpha":
        dist_params = {"alpha": args.alpha, "minority_dist": "zipfian"}
    elif args.distribution == "zipfian":
        dist_params = {"exponent": args.exponent}
    else:
        dist_params = {}

    filter_params = {"fingerprint_bits": args.fingerprint_bits}

    if args.sweep:
        configs = build_sweep_configs(args.prefilter, filter_params,
                                      args.shared_codebook, args.shared_filter)
    else:
        configs = [{
            "num_rows": args.num_rows,
            "num_cols": args.num_cols,
            "vocab_size": args.vocab_size,
            "distribution": args.distribution,
            "dist_params": dist_params,
            "permutation": args.permutation,
            "permutation_params": {},
            "prefilter": args.prefilter,
            "filter_params": filter_params,
            "shared_codebook": args.shared_codebook,
            "shared_filter": args.shared_filter,
        }]

    total = len(configs)
    print(f"CaramelDB Multiset Benchmark ({total} configs)")
    print("=" * 60)

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
    print("\n" + "=" * 60)
    header = f"{'rows':>8} {'cols':>5} {'distribution':<20} {'permutation':<14} {'build(s)':>9} {'bits/key':>9} {'query(ns)':>10}"
    print(f"\nSummary:\n{header}")
    print("-" * len(header))
    for r in results:
        dist_label = r["distribution"]
        if r.get("prefilter"):
            dist_label += f"+{r['prefilter']}"
        print(
            f"{r['num_rows']:>8} {r['num_cols']:>5} {dist_label:<20} "
            f"{r['permutation']:<14} {r['construction_s']:>9.2f} "
            f"{r['bits_per_key']:>9.3f} {r['query_ns']:>10.1f}"
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
