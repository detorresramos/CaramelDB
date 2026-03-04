"""Data generation for Shibuya comparison. Builds CSFs and saves results to JSON.

For each (alpha, distribution), both methods decide:
  1. Whether to use a prefilter at all
  2. If yes, what Bloom parameters (bits_per_element, num_hashes) to use

We build each recommendation and measure actual bits/key.

Usage:
    python shibuya_comparison/run_experiments.py
"""

import json
import os
import sys

import carameldb
import numpy as np
from carameldb import BloomFilterConfig
from tqdm import tqdm

_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_dir, ".."))

from shared.data_gen import compute_actual_alpha, gen_alpha_values, gen_keys
from shared.measure import measure_csf
from shared.shibuya import empirical_entropy, shibuya_bloom_params
from shared.theory import best_discrete_bloom_all_k

N = 100_000
SEED = 42
ALPHAS = list(np.arange(0.50, 1.00, 0.01))
DISTS = ["unique", "zipfian", "uniform_100"]

FIGURES_DIR = os.path.join(_dir, "figures")
DATA_DIR = os.path.join(FIGURES_DIR, "data")


def build_and_measure_bpk(keys, values, prefilter=None):
    structure = carameldb.Caramel(keys, values, prefilter=prefilter, verbose=False)
    return structure.get_stats().in_memory_bytes * 8 / len(keys)


def our_recommendation(alpha, n_over_N):
    """Pick the (bits_per_element, num_hashes) maximizing the lower bound.

    Returns None if no config has positive lower bound.
    """
    bpe, k, lb = best_discrete_bloom_all_k(alpha, n_over_N)
    if lb <= 0:
        return None
    return bpe, k


def run_experiments():
    results = {}
    for dist in DISTS:
        rows = []
        for alpha in tqdm(ALPHAS, desc=f"shibuya {dist}"):
            keys = gen_keys(N)
            values = gen_alpha_values(N, alpha, seed=SEED, minority_dist=dist)

            actual_alpha = compute_actual_alpha(values)
            H0 = empirical_entropy(values)

            baseline = measure_csf(keys, values, "none", minority_dist=dist)
            n_over_N = baseline.huffman_num_symbols / N
            baseline_bpk = baseline.bits_per_key

            our_rec = our_recommendation(actual_alpha, n_over_N)
            shib_rec = shibuya_bloom_params(actual_alpha, H0)

            if our_rec is None:
                our_bpk = baseline_bpk
            else:
                config = BloomFilterConfig(bits_per_element=our_rec[0], num_hashes=our_rec[1])
                our_bpk = build_and_measure_bpk(keys, values, prefilter=config)

            if shib_rec is None:
                shib_bpk = baseline_bpk
            else:
                config = BloomFilterConfig(bits_per_element=shib_rec[0], num_hashes=shib_rec[1])
                shib_bpk = build_and_measure_bpk(keys, values, prefilter=config)

            rows.append(
                {
                    "alpha": round(actual_alpha, 4),
                    "requested_alpha": round(alpha, 4),
                    "n_over_N": n_over_N,
                    "H0": H0,
                    "baseline_bpk": baseline_bpk,
                    "our_bpk": our_bpk,
                    "our_params": list(our_rec) if our_rec else None,
                    "shib_bpk": shib_bpk,
                    "shib_params": list(shib_rec) if shib_rec else None,
                }
            )
            print(
                f"  {dist} a={actual_alpha:.4f}: baseline={baseline_bpk:.2f}  "
                f"ours={our_bpk:.2f} {our_rec}  "
                f"shibuya={shib_bpk:.2f} {shib_rec}"
            )

        results[dist] = rows
    return results


def save_json(data, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        json.dump(data, f, indent=2)
    print(f"Saved: {path}")


def main():
    os.makedirs(DATA_DIR, exist_ok=True)
    results = run_experiments()
    data = {
        "N": N,
        "seed": SEED,
        "distributions": results,
    }
    save_json(data, os.path.join(DATA_DIR, "shibuya_comparison.json"))


if __name__ == "__main__":
    main()
