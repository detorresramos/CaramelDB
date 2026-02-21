"""Experiment runner for baseline comparisons.

Measures construction time, inference time, and memory usage for each method
across configurable dataset parameters. Saves results as JSON.

Usage:
    python experiments/baselines/run_baselines.py                              # default sweep
    python experiments/baselines/run_baselines.py --alpha 0.8 --n 100000      # single config
    python experiments/baselines/run_baselines.py --filter-type binary_fuse    # specify filter
    python experiments/baselines/run_baselines.py --dataset data.csv           # custom dataset CSV
"""

import argparse
import csv
import json
import os
import sys
import time

import numpy as np

_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _dir)
sys.path.insert(0, os.path.join(_dir, ".."))

from data_gen import MINORITY_DISTRIBUTIONS, compute_actual_alpha, gen_alpha_values, gen_keys
from methods import CSFFilter, HashTable

FIGURES_DIR = os.path.join(_dir, "figures")
DATA_DIR = os.path.join(FIGURES_DIR, "data")

DEFAULT_N = 100_000
DEFAULT_SEED = 42
DEFAULT_FILTER_TYPE = "binary_fuse"
NUM_INFERENCE_QUERIES = 100

SWEEP_CONFIGS = [
    {"alpha": 0.7, "minority_dist": "unique"},
    {"alpha": 0.8, "minority_dist": "unique"},
    {"alpha": 0.9, "minority_dist": "unique"},
    {"alpha": 0.8, "minority_dist": "zipfian"},
    {"alpha": 0.8, "minority_dist": "uniform_100"},
]


def measure_inference_time(method, structure, keys, seed):
    """Measure average inference time over random key lookups."""
    rng = np.random.RandomState(seed)
    sample_indices = rng.choice(len(keys), size=NUM_INFERENCE_QUERIES, replace=True)
    sample_keys = [keys[i] for i in sample_indices]

    # Warmup
    for k in sample_keys[:10]:
        method.query(structure, k)

    times_ns = []
    for k in sample_keys:
        t0 = time.perf_counter_ns()
        method.query(structure, k)
        t1 = time.perf_counter_ns()
        times_ns.append(t1 - t0)

    return float(np.mean(times_ns))


def run_single_method(method, keys, values, seed):
    """Run a single method and collect all measurements."""
    # Construction time
    t0 = time.perf_counter()
    structure = method.construct(keys, values)
    construction_time = time.perf_counter() - t0

    # Memory
    if hasattr(method, "measure_memory_from_structure") and method.measure_memory(keys, values) is None:
        memory_bytes = method.measure_memory_from_structure(structure)
    else:
        memory_bytes = method.measure_memory(keys, values)

    # Inference time
    avg_inference_ns = measure_inference_time(method, structure, keys, seed)

    return {
        "method": method.name,
        "construction_time_s": construction_time,
        "avg_inference_time_ns": avg_inference_ns,
        "memory_bytes": memory_bytes,
        "filter_params": method.get_params(),
    }


def load_dataset(path):
    """Load a dataset from a CSV file with 'key' and 'value' columns."""
    keys = []
    values = []
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            keys.append(row["key"])
            values.append(int(row["value"]))
    return keys, np.array(values, dtype=np.uint32)


def run_experiment(n, alpha, minority_dist, seed, filter_type, keys=None, values=None):
    """Run all methods for a single dataset configuration."""
    if keys is None:
        keys = gen_keys(n, seed=seed)
        values = gen_alpha_values(n, alpha, seed=seed, minority_dist=minority_dist)
    actual_alpha = compute_actual_alpha(values)

    methods = [
        HashTable(),
        CSFFilter(filter_type=filter_type, epsilon_strategy="optimal"),
        CSFFilter(filter_type=filter_type, epsilon_strategy="shibuya"),
    ]

    results = []
    for method in methods:
        print(f"  Running {method.name}...")
        result = run_single_method(method, keys, values, seed)
        result["filter_type"] = filter_type if method.name != "hash_table" else None
        results.append(result)
        print(f"    construction={result['construction_time_s']:.3f}s  "
              f"inference={result['avg_inference_time_ns']:.0f}ns  "
              f"memory={result['memory_bytes']:,}B")

    return {
        "dataset": {
            "N": len(keys),
            "alpha": round(actual_alpha, 4),
            "target_alpha": alpha,
            "minority_dist": minority_dist,
            "seed": seed,
        },
        "results": results,
    }


def save_json(data, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        json.dump(data, f, indent=2)
    print(f"Saved: {path}")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Run baseline comparisons",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--n", type=int, default=DEFAULT_N, help="Number of keys")
    parser.add_argument("--alpha", type=float, default=None, help="Alpha value (omit for sweep)")
    parser.add_argument("--minority-dist", choices=MINORITY_DISTRIBUTIONS, default=None,
                        help="Minority distribution (omit for sweep)")
    parser.add_argument("--dataset", type=str, default=None,
                        help="Path to CSV file with 'key' and 'value' columns")
    parser.add_argument("--filter-type", choices=["xor", "binary_fuse", "bloom"],
                        default=DEFAULT_FILTER_TYPE, help="Filter type for CSF methods")
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED, help="Random seed")
    return parser.parse_args()


def main():
    args = parse_args()
    os.makedirs(DATA_DIR, exist_ok=True)

    if args.dataset is not None:
        # Custom dataset from CSV
        keys, values = load_dataset(args.dataset)
        alpha = float(compute_actual_alpha(values))
        n = len(keys)
        dataset_name = os.path.splitext(os.path.basename(args.dataset))[0]
        print(f"\n=== dataset={args.dataset}, n={n}, alpha={alpha:.4f}, filter={args.filter_type} ===")

        data = run_experiment(n, alpha, "custom", args.seed, args.filter_type,
                              keys=keys, values=values)
        data["dataset"]["source"] = args.dataset

        filename = f"baselines_{dataset_name}_{args.filter_type}.json"
        save_json(data, os.path.join(DATA_DIR, filename))
        return

    if args.alpha is not None:
        # Single experiment
        minority_dist = args.minority_dist or "unique"
        configs = [{"alpha": args.alpha, "minority_dist": minority_dist}]
    else:
        # Default sweep
        configs = SWEEP_CONFIGS

    all_results = []
    for cfg in configs:
        alpha = cfg["alpha"]
        dist = cfg["minority_dist"]
        print(f"\n=== alpha={alpha}, dist={dist}, n={args.n}, filter={args.filter_type} ===")

        data = run_experiment(args.n, alpha, dist, args.seed, args.filter_type)
        all_results.append(data)

        filename = f"baselines_a{alpha}_{dist}_{args.filter_type}.json"
        save_json(data, os.path.join(DATA_DIR, filename))

    # Also save combined results
    combined = {
        "filter_type": args.filter_type,
        "N": args.n,
        "seed": args.seed,
        "experiments": all_results,
    }
    save_json(combined, os.path.join(DATA_DIR, f"baselines_combined_{args.filter_type}.json"))


if __name__ == "__main__":
    main()
