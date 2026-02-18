"""Data generation for paper plots. Builds CSFs and saves results to JSON."""

import json
import os
import sys

import numpy as np
from tqdm import tqdm

_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _dir)
sys.path.insert(0, os.path.join(_dir, ".."))

import theory
from data_gen import gen_alpha_values, gen_keys
from measure import measure_csf

N = 100_000
SEED = 42
DISTRIBUTIONS = ["unique", "zipfian", "uniform_100"]
ALPHAS = list(np.arange(0.50, 1.00, 0.01))
EPSILON_SWEEP_ALPHAS = [0.7, 0.9]

FIGURES_DIR = os.path.join(_dir, "figures")
DATA_DIR = os.path.join(FIGURES_DIR, "data")

FILTERS = {
    "xor": {
        "filter_type": "xor",
        "param_range": range(1, 9),
        "param_key": "fingerprint_bits",
        "theory_best_fn": theory.best_discrete_xor,
        "measure_kwargs": lambda p: {"fingerprint_bits": p},
    },
    "binary_fuse": {
        "filter_type": "binary_fuse",
        "param_range": range(1, 9),
        "param_key": "fingerprint_bits",
        "theory_best_fn": theory.best_discrete_binary_fuse,
        "measure_kwargs": lambda p: {"fingerprint_bits": p},
    },
    "bloom_k1": {
        "filter_type": "bloom",
        "param_range": range(1, 9),
        "param_key": "bpe",
        "theory_best_fn": lambda a, n: theory.best_discrete_bloom(a, n, k=1),
        "measure_kwargs": lambda p: {"bloom_bits_per_element": p, "bloom_num_hashes": 1},
    },
    "bloom_k2": {
        "filter_type": "bloom",
        "param_range": range(1, 9),
        "param_key": "bpe",
        "theory_best_fn": lambda a, n: theory.best_discrete_bloom(a, n, k=2),
        "measure_kwargs": lambda p: {"bloom_bits_per_element": p, "bloom_num_hashes": 2},
    },
    "bloom_k3": {
        "filter_type": "bloom",
        "param_range": range(1, 9),
        "param_key": "bpe",
        "theory_best_fn": lambda a, n: theory.best_discrete_bloom(a, n, k=3),
        "measure_kwargs": lambda p: {"bloom_bits_per_element": p, "bloom_num_hashes": 3},
    },
}


def run_alpha_sweep(filter_label, dist):
    cfg = FILTERS[filter_label]
    results = []

    for alpha in tqdm(ALPHAS, desc=f"alpha sweep {filter_label}/{dist}"):
        keys = gen_keys(N)
        values = gen_alpha_values(N, alpha, seed=SEED, minority_dist=dist)

        baseline = measure_csf(keys, values, "none", N, alpha, minority_dist=dist)
        baseline_bpk = baseline.bits_per_key
        n_over_N = baseline.huffman_num_symbols / N

        best_param, _ = cfg["theory_best_fn"](alpha, n_over_N)

        empirical_per_param = []
        for param in cfg["param_range"]:
            kwargs = cfg["measure_kwargs"](param)
            filtered = measure_csf(
                keys, values, cfg["filter_type"], N, alpha,
                minority_dist=dist, **kwargs,
            )
            bpk_saved = baseline_bpk - filtered.bits_per_key
            empirical_per_param.append({cfg["param_key"]: param, "bpk_saved": bpk_saved})

        theory_guided = next(
            r for r in empirical_per_param if r[cfg["param_key"]] == best_param
        )
        best_empirical = max(empirical_per_param, key=lambda r: r["bpk_saved"])

        results.append({
            "alpha": round(alpha, 4),
            "n_over_N": n_over_N,
            "baseline_bpk": baseline_bpk,
            "theory_optimal_params": {cfg["param_key"]: best_param},
            "theory_guided_bpk_saved": theory_guided["bpk_saved"],
            "empirical_per_param": empirical_per_param,
            "best_empirical_bpk_saved": best_empirical["bpk_saved"],
            "best_empirical_params": {cfg["param_key"]: best_empirical[cfg["param_key"]]},
        })

    return {
        "filter_type": filter_label,
        "distribution": dist,
        "N": N,
        "alphas": [round(a, 4) for a in ALPHAS],
        "results": results,
    }


def run_epsilon_sweep(filter_label, dist, alpha):
    cfg = FILTERS[filter_label]

    keys = gen_keys(N)
    values = gen_alpha_values(N, alpha, seed=SEED, minority_dist=dist)

    baseline = measure_csf(keys, values, "none", N, alpha, minority_dist=dist)
    baseline_bpk = baseline.bits_per_key
    n_over_N = baseline.huffman_num_symbols / N

    empirical_per_param = []
    for param in tqdm(cfg["param_range"], desc=f"eps sweep {filter_label}/{dist}/a={alpha}"):
        kwargs = cfg["measure_kwargs"](param)
        filtered = measure_csf(
            keys, values, cfg["filter_type"], N, alpha,
            minority_dist=dist, **kwargs,
        )
        bpk_saved = baseline_bpk - filtered.bits_per_key
        empirical_per_param.append({cfg["param_key"]: param, "bpk_saved": bpk_saved})

    return {
        "filter_type": filter_label,
        "distribution": dist,
        "alpha": alpha,
        "N": N,
        "n_over_N": n_over_N,
        "baseline_bpk": baseline_bpk,
        "empirical_per_param": empirical_per_param,
    }


def save_json(data, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        json.dump(data, f, indent=2)
    print(f"Saved: {path}")


def main():
    os.makedirs(DATA_DIR, exist_ok=True)

    for filter_label in FILTERS:
        for dist in DISTRIBUTIONS:
            path = os.path.join(DATA_DIR, f"alpha_sweep_{filter_label}_{dist}.json")
            data = run_alpha_sweep(filter_label, dist)
            save_json(data, path)

    for filter_label in FILTERS:
        for dist in DISTRIBUTIONS:
            for alpha in EPSILON_SWEEP_ALPHAS:
                path = os.path.join(
                    DATA_DIR, f"epsilon_sweep_{filter_label}_{dist}_alpha{alpha}.json"
                )
                data = run_epsilon_sweep(filter_label, dist, alpha)
                save_json(data, path)


if __name__ == "__main__":
    main()
