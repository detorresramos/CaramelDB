"""Plot generation for Shibuya comparison. Reads JSON data, renders plots.

Usage:
    python shibuya_comparison/make_plots.py
"""

import json
import os
import sys

import matplotlib.pyplot as plt

_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_dir, ".."))

FIGURES_DIR = os.path.join(_dir, "figures")
DATA_DIR = os.path.join(FIGURES_DIR, "data")

DISTS = ["unique", "zipfian", "uniform_100"]
DIST_LABELS = {"unique": "Unique", "zipfian": "Zipfian", "uniform_100": "Uniform-100"}


def load_json(path):
    if not os.path.exists(path):
        return None
    with open(path) as f:
        return json.load(f)


def plot_bits_per_key(data):
    N = data["N"]
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.5), sharey=True)

    for ax, dist in zip(axes, DISTS):
        rows = data["distributions"][dist]
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
            label="AutoCSF",
        )
        ax.plot(
            alphas,
            [r["shib_bpk"] for r in rows],
            "--",
            color="tab:orange",
            linewidth=2,
            label="Heuristic",
        )
        ax.set_xlabel(r"$\alpha$")
        ax.set_title(DIST_LABELS[dist])
        ax.legend(loc="best")
        ax.grid(True, alpha=0.3)

    axes[0].set_ylabel("Bits per key")
    fig.suptitle(
        f"Measured bits/key: Bloom filter recommendation comparison (N={N:,})",
        fontsize=14,
        y=1.02,
    )
    plt.tight_layout()
    return fig


def plot_bits_per_key_saved(data):
    N = data["N"]
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.5), sharey=True)

    for ax, dist in zip(axes, DISTS):
        rows = data["distributions"][dist]
        alphas = [r["alpha"] for r in rows]

        ax.plot(
            alphas,
            [r["baseline_bpk"] - r["our_bpk"] for r in rows],
            "-",
            color="tab:blue",
            linewidth=2,
            label="AutoCSF",
        )
        ax.plot(
            alphas,
            [r["baseline_bpk"] - r["shib_bpk"] for r in rows],
            "--",
            color="tab:orange",
            linewidth=2,
            label="Heuristic",
        )
        ax.axhline(0, color="black", linewidth=0.5, linestyle=":")
        ax.set_xlabel(r"$\alpha$")
        ax.set_title(DIST_LABELS[dist])
        ax.legend(loc="best")
        ax.grid(True, alpha=0.3)

    axes[0].set_ylabel("Bits/key saved vs no filter")
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

    path = os.path.join(DATA_DIR, "shibuya_comparison.json")
    data = load_json(path)
    if data is None:
        print(f"No data found at {path}. Run run_experiments.py first.")
        return

    os.makedirs(FIGURES_DIR, exist_ok=True)

    for name, plot_fn in [
        ("bloom_bits_per_key", plot_bits_per_key),
        ("bloom_bits_per_key_saved", plot_bits_per_key_saved),
    ]:
        fig = plot_fn(data)
        out = os.path.join(FIGURES_DIR, f"{name}.png")
        fig.savefig(out, dpi=300, bbox_inches="tight")
        plt.close(fig)
        print(f"  Saved: {out}")


if __name__ == "__main__":
    main()
