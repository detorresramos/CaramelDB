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

import carameldb
import matplotlib.pyplot as plt
import numpy as np
from carameldb import BloomFilterConfig

_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_dir, ".."))

from data_gen import compute_actual_alpha, gen_alpha_values, gen_keys
from shibuya import empirical_entropy, shibuya_bloom_params
from theory import best_discrete_bloom

FIGURES_DIR = os.path.join(_dir, "figures")
ALPHAS = np.arange(0.50, 1.00, 0.01)
DISTS = ["unique", "zipfian", "uniform_100"]
DIST_LABELS = {"unique": "Unique", "zipfian": "Zipfian", "uniform_100": "Uniform(100)"}
N = 100_000
SEED = 42


def build_and_measure_bpk(keys, values, prefilter=None):
    structure = carameldb.Caramel(keys, values, prefilter=prefilter, verbose=False)
    return structure.get_stats().in_memory_bytes * 8 / len(keys)


def our_recommendation(alpha, n_over_N):
    """Pick the (bits_per_element, num_hashes) maximizing the lower bound.

    Returns None if no config has positive lower bound.
    """
    best_lb, best_bpe, best_nh = float("-inf"), None, None
    for num_hashes in range(1, 9):
        bpe, lb = best_discrete_bloom(alpha, n_over_N, num_hashes)
        if lb > best_lb:
            best_lb, best_bpe, best_nh = lb, bpe, num_hashes
    if best_lb <= 0:
        return None
    return best_bpe, best_nh


def _measure_with_recommendation(keys, values, rec, baseline_bpk):
    if rec is None:
        return baseline_bpk
    config = BloomFilterConfig(bits_per_element=rec[0], num_hashes=rec[1])
    return build_and_measure_bpk(keys, values, prefilter=config)


def run_experiments():
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
            shib_rec = shibuya_bloom_params(actual_alpha, H0)

            our_bpk = _measure_with_recommendation(keys, values, our_rec, baseline_bpk)
            shib_bpk = _measure_with_recommendation(
                keys, values, shib_rec, baseline_bpk
            )

            rows.append(
                {
                    "alpha": actual_alpha,
                    "baseline_bpk": baseline_bpk,
                    "our_bpk": our_bpk,
                    "our_params": our_rec,
                    "shib_bpk": shib_bpk,
                    "shib_params": shib_rec,
                }
            )
            print(
                f"  {dist} a={alpha:.2f}: baseline={baseline_bpk:.2f}  "
                f"ours={our_bpk:.2f} {our_rec}  "
                f"shibuya={shib_bpk:.2f} {shib_rec}"
            )

        results[dist] = rows
    return results


def plot_bits_per_key(results):
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.5), sharey=True)

    for ax, dist in zip(axes, DISTS):
        rows = results[dist]
        alphas = [r["alpha"] for r in rows]

        ax.plot(
            alphas,
            [r["baseline_bpk"] for r in rows],
            "-",
            color="tab:gray",
            linewidth=1.5,
            label="No filter",
        )
        ax.plot(
            alphas,
            [r["our_bpk"] for r in rows],
            "-",
            color="tab:blue",
            linewidth=2,
            label="Ours",
        )
        ax.plot(
            alphas,
            [r["shib_bpk"] for r in rows],
            "--",
            color="tab:orange",
            linewidth=2,
            label="Shibuya",
        )
        ax.set_xlabel(r"$\alpha$")
        ax.set_title(DIST_LABELS[dist])
        ax.grid(True, alpha=0.3)

    axes[0].set_ylabel("Bits per key")
    axes[-1].legend(loc="best")
    fig.suptitle(
        f"Measured bits/key: Bloom filter recommendation comparison (N={N:,})",
        fontsize=14,
        y=1.02,
    )
    plt.tight_layout()
    return fig


def plot_bits_per_key_saved(results):
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.5), sharey=True)

    for ax, dist in zip(axes, DISTS):
        rows = results[dist]
        alphas = [r["alpha"] for r in rows]

        ax.plot(
            alphas,
            [r["baseline_bpk"] - r["our_bpk"] for r in rows],
            "-",
            color="tab:blue",
            linewidth=2,
            label="Ours",
        )
        ax.plot(
            alphas,
            [r["baseline_bpk"] - r["shib_bpk"] for r in rows],
            "--",
            color="tab:orange",
            linewidth=2,
            label="Shibuya",
        )
        ax.axhline(0, color="black", linewidth=0.5, linestyle=":")
        ax.set_xlabel(r"$\alpha$")
        ax.set_title(DIST_LABELS[dist])
        ax.grid(True, alpha=0.3)

    axes[0].set_ylabel("Bits/key saved vs no filter")
    axes[-1].legend(loc="best")
    fig.suptitle(
        f"Measured bits/key saved: Bloom filter recommendation comparison (N={N:,})",
        fontsize=14,
        y=1.02,
    )
    plt.tight_layout()
    return fig


def main():
    plt.rcParams.update(
        {
            "font.size": 10,
            "axes.labelsize": 11,
            "axes.titlesize": 12,
            "legend.fontsize": 9,
            "xtick.labelsize": 9,
            "ytick.labelsize": 9,
            "lines.linewidth": 1.5,
        }
    )

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
