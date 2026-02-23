"""Empirical comparison: our Bloom filter recommendation vs Shibuya's.

For each (alpha, distribution), both methods decide:
  1. Whether to use a prefilter at all
  2. If yes, what Bloom parameters (bits_per_element, num_hashes) to use

We build each recommendation and measure actual bits/key.

Usage:
    python experiments/shibuya_comparison/make_plots.py
"""

import os
import sys

import matplotlib.pyplot as plt
import numpy as np

import carameldb
from carameldb import BloomFilterConfig

_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_dir, ".."))

from data_gen import compute_actual_alpha, gen_alpha_values, gen_keys
from shibuya import empirical_entropy, shibuya_best_discrete_params, shibuya_optimal_epsilon
from theory import best_discrete_bloom

FIGURES_DIR = os.path.join(_dir, "figures")
ALPHAS = np.arange(0.50, 1.00, 0.01)
DISTS = ["unique", "zipfian", "uniform_100"]
DIST_LABELS = {"unique": "Unique", "zipfian": "Zipfian", "uniform_100": "Uniform(100)"}
N = 100_000
SEED = 42


def build_and_measure_bpk(keys, values, prefilter=None):
    """Build a Caramel structure and return bits per key."""
    structure = carameldb.Caramel(keys, values, prefilter=prefilter, verbose=False)
    return structure.get_stats().in_memory_bytes * 8 / len(keys)


def our_recommendation(alpha, n_over_N):
    """Try all (bits_per_element, num_hashes), pick the one maximizing lower bound.

    Returns (bits_per_element, num_hashes) or None if no config has positive lower bound.
    """
    best_lb, best_bits_per_element, best_num_hashes = float("-inf"), None, None
    for num_hashes in range(1, 9):
        bits_per_element, lb = best_discrete_bloom(alpha, n_over_N, num_hashes)
        if lb > best_lb:
            best_lb = lb
            best_bits_per_element = bits_per_element
            best_num_hashes = num_hashes
    if best_lb <= 0:
        return None
    return best_bits_per_element, best_num_hashes


def shibuya_recommendation(alpha, H0):
    """Shibuya's method: if eps* < 1, find closest (bits_per_element, num_hashes).

    Returns (bits_per_element, num_hashes) or None if eps* >= 1 (no filter recommended).
    """
    eps_star = shibuya_optimal_epsilon(alpha, H0)
    if eps_star >= 1:
        return None
    result = shibuya_best_discrete_params(alpha, H0, "bloom")
    return result["bloom_bits_per_element"], result["bloom_num_hashes"]


def run_experiments():
    """For each (alpha, dist), build baseline/ours/shibuya and measure bits/key."""
    results = {}
    for dist in DISTS:
        rows = []
        for alpha in ALPHAS:
            keys = gen_keys(N, seed=SEED)
            values = gen_alpha_values(N, alpha, seed=SEED, minority_dist=dist)
            actual_alpha = compute_actual_alpha(values)
            H0 = empirical_entropy(values)
            n_over_N = len(np.unique(values)) / N

            baseline_bpk = build_and_measure_bpk(keys, values)

            our_rec = our_recommendation(actual_alpha, n_over_N)
            if our_rec is not None:
                config = BloomFilterConfig(bits_per_element=our_rec[0], num_hashes=our_rec[1])
                our_bpk = build_and_measure_bpk(keys, values, prefilter=config)
            else:
                our_bpk = baseline_bpk

            shib_rec = shibuya_recommendation(actual_alpha, H0)
            if shib_rec is not None:
                config = BloomFilterConfig(bits_per_element=shib_rec[0], num_hashes=shib_rec[1])
                shib_bpk = build_and_measure_bpk(keys, values, prefilter=config)
            else:
                shib_bpk = baseline_bpk

            rows.append({
                "alpha": actual_alpha,
                "baseline_bpk": baseline_bpk,
                "our_bpk": our_bpk,
                "our_params": our_rec,
                "shib_bpk": shib_bpk,
                "shib_params": shib_rec,
            })
            print(f"  {dist} a={alpha:.2f}: baseline={baseline_bpk:.2f}  "
                  f"ours={our_bpk:.2f} {our_rec}  "
                  f"shibuya={shib_bpk:.2f} {shib_rec}")

        results[dist] = rows
    return results


def plot_bits_per_key(results):
    """Plot measured bits/key for baseline, ours, and Shibuya."""
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.5), sharey=True)

    for ax, dist in zip(axes, DISTS):
        rows = results[dist]
        alphas = [r["alpha"] for r in rows]
        baseline = [r["baseline_bpk"] for r in rows]
        ours = [r["our_bpk"] for r in rows]
        shibuya = [r["shib_bpk"] for r in rows]

        ax.plot(alphas, baseline, "-", color="tab:gray", linewidth=1.5, label="No filter")
        ax.plot(alphas, ours, "-", color="tab:blue", linewidth=2, label="Ours")
        ax.plot(alphas, shibuya, "--", color="tab:orange", linewidth=2, label="Shibuya")
        ax.set_xlabel(r"$\alpha$")
        ax.set_title(DIST_LABELS[dist])
        ax.grid(True, alpha=0.3)

    axes[0].set_ylabel("Bits per key")
    axes[-1].legend(loc="best")
    fig.suptitle(f"Measured bits/key: Bloom filter recommendation comparison (N={N:,})", fontsize=14, y=1.02)
    plt.tight_layout()
    return fig


def plot_bits_per_key_saved(results):
    """Plot measured bits/key saved vs baseline for ours and Shibuya."""
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.5), sharey=True)

    for ax, dist in zip(axes, DISTS):
        rows = results[dist]
        alphas = [r["alpha"] for r in rows]
        our_saved = [r["baseline_bpk"] - r["our_bpk"] for r in rows]
        shib_saved = [r["baseline_bpk"] - r["shib_bpk"] for r in rows]

        ax.plot(alphas, our_saved, "-", color="tab:blue", linewidth=2, label="Ours")
        ax.plot(alphas, shib_saved, "--", color="tab:orange", linewidth=2, label="Shibuya")
        ax.axhline(0, color="black", linewidth=0.5, linestyle=":")
        ax.set_xlabel(r"$\alpha$")
        ax.set_title(DIST_LABELS[dist])
        ax.grid(True, alpha=0.3)

    axes[0].set_ylabel("Bits/key saved vs no filter")
    axes[-1].legend(loc="best")
    fig.suptitle(f"Measured bits/key saved: Bloom filter recommendation comparison (N={N:,})", fontsize=14, y=1.02)
    plt.tight_layout()
    return fig


def main():
    plt.rcParams.update({
        "font.size": 10,
        "axes.labelsize": 11,
        "axes.titlesize": 12,
        "legend.fontsize": 9,
        "xtick.labelsize": 9,
        "ytick.labelsize": 9,
        "lines.linewidth": 1.5,
    })

    print("Running experiments...")
    results = run_experiments()

    os.makedirs(FIGURES_DIR, exist_ok=True)

    for name, plot_fn in [
        ("bloom_bits_per_key", plot_bits_per_key),
        ("bloom_bits_per_key_saved", plot_bits_per_key_saved),
    ]:
        fig = plot_fn(results)
        out = os.path.join(FIGURES_DIR, f"{name}.png")
        fig.savefig(out, dpi=300, bbox_inches="tight")
        plt.close(fig)
        print(f"  Saved: {out}")


if __name__ == "__main__":
    main()
