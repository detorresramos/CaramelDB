"""Plot generation for baseline comparisons.

Usage:
    python experiments/baselines/make_plots.py
    python experiments/baselines/make_plots.py --filter-type binary_fuse
"""

import argparse
import json
import os
import sys

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.lines import Line2D

_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _dir)

FIGURES_DIR = os.path.join(_dir, "figures")
DATA_DIR = os.path.join(FIGURES_DIR, "data")

METHOD_DISPLAY = {
    "hash_table": "Hash Table",
    "csf_filter_optimal_xor": "CSF+XOR (Optimal)",
    "csf_filter_optimal_binary_fuse": "CSF+BinaryFuse (Optimal)",
    "csf_filter_optimal_bloom": "CSF+Bloom (Optimal)",
    "csf_filter_shibuya_xor": "CSF+XOR (Shibuya)",
    "csf_filter_shibuya_binary_fuse": "CSF+BinaryFuse (Shibuya)",
    "csf_filter_shibuya_bloom": "CSF+Bloom (Shibuya)",
}

METHOD_MARKERS = {
    "hash_table": "s",
    "csf_filter_optimal": "o",
    "csf_filter_shibuya": "^",
}

METHOD_COLORS = {
    "hash_table": "tab:gray",
    "csf_filter_optimal": "tab:blue",
    "csf_filter_shibuya": "tab:orange",
}


def load_json(path):
    if not os.path.exists(path):
        return None
    with open(path) as f:
        return json.load(f)


def _format_n(n):
    if n >= 1_000_000:
        return f"{n // 1_000_000}M"
    elif n >= 1_000:
        return f"{n // 1_000}k"
    return str(n)


def _method_style(method_name):
    for prefix in METHOD_MARKERS:
        if method_name.startswith(prefix) or method_name == prefix:
            return METHOD_MARKERS[prefix], METHOD_COLORS[prefix]
    return "o", "tab:blue"


def build_grid(experiments):
    grid = {}
    for exp in experiments:
        ds = exp["dataset"]
        key = (ds["minority_dist"], ds["N"], ds["alpha"])
        grid[key] = exp
    return grid


def filter_by_dist(experiments, dist):
    return [e for e in experiments if e["dataset"]["minority_dist"] == dist]


def print_table(title, experiments, value_fn, fmt=".1f"):
    dists = sorted(set(exp["dataset"]["minority_dist"] for exp in experiments))
    grid = build_grid(experiments)

    for dist in dists:
        dist_exps = filter_by_dist(experiments, dist)
        ns = sorted(set(exp["dataset"]["N"] for exp in dist_exps))
        alphas = sorted(set(exp["dataset"]["alpha"] for exp in dist_exps))

        methods = []
        for exp in dist_exps:
            for r in exp["results"]:
                if r["method"] not in methods:
                    methods.append(r["method"])

        print(f"\n{'=' * 40}")
        print(f"  {title} [{dist}]")
        print(f"{'=' * 40}")

        for n in ns:
            col_labels = [f"a={a}" for a in alphas]
            header = f"N={_format_n(n):<6} {'Method':<25}"
            for label in col_labels:
                header += f"  {label:>8}"
            print(header)
            print("-" * (31 + 10 * len(col_labels)))

            for method in methods:
                row = f"       {METHOD_DISPLAY.get(method, method):<25}"
                for a in alphas:
                    exp = grid.get((dist, n, a))
                    if exp is None:
                        row += f"  {'—':>8}"
                        continue
                    match = next(
                        (r for r in exp["results"] if r["method"] == method), None
                    )
                    if match:
                        val = value_fn(match, exp["dataset"])
                        row += f"  {val:>8{fmt}}"
                    else:
                        row += f"  {'—':>8}"
                print(row)
            print()


def plot_inference_vs_memory_grid(experiments, filter_type):
    ns = sorted(set(exp["dataset"]["N"] for exp in experiments))
    alphas = sorted(set(exp["dataset"]["alpha"] for exp in experiments))
    grid = build_grid(experiments)

    fig, axes = plt.subplots(1, len(ns), figsize=(6 * len(ns), 5), squeeze=False)
    axes = axes[0]

    cmap = plt.cm.viridis
    alpha_norm = plt.Normalize(min(alphas), max(alphas))

    for ax_idx, n in enumerate(ns):
        ax = axes[ax_idx]

        for alpha in alphas:
            exp = grid.get((n, alpha))
            if exp is None:
                continue
            N = exp["dataset"]["N"]
            color = cmap(alpha_norm(alpha))

            for r in exp["results"]:
                marker, _ = _method_style(r["method"])
                ax.scatter(
                    r["memory_bytes"] * 8 / N,
                    r["avg_inference_time_ns"],
                    s=80,
                    marker=marker,
                    color=color,
                    zorder=5,
                    edgecolors="white",
                    linewidths=0.5,
                )

        ax.set_xlabel("Memory (bits/key)")
        ax.set_ylabel("Avg inference time (ns)")
        ax.set_title(f"N = {_format_n(n)}")
        ax.grid(True, alpha=0.3)

    method_handles = [
        Line2D(
            [0],
            [0],
            marker=marker,
            color="gray",
            linestyle="None",
            markersize=8,
            label=prefix.replace("_", " ").title(),
        )
        for prefix, marker in METHOD_MARKERS.items()
    ]
    axes[-1].legend(handles=method_handles, fontsize=8, loc="best")

    sm = plt.cm.ScalarMappable(cmap=cmap, norm=alpha_norm)
    sm.set_array([])
    fig.colorbar(sm, ax=axes, label=r"$\alpha$", shrink=0.8)

    fig.suptitle(f"Inference vs Memory — {filter_type}", fontsize=14, y=1.02)
    plt.tight_layout()
    return fig


def plot_memory_vs_alpha(experiments, filter_type):
    ns = sorted(set(exp["dataset"]["N"] for exp in experiments))
    alphas = sorted(set(exp["dataset"]["alpha"] for exp in experiments))
    grid = build_grid(experiments)

    methods = []
    for exp in experiments:
        for r in exp["results"]:
            if r["method"] not in methods:
                methods.append(r["method"])

    fig, ax = plt.subplots(figsize=(10, 6))
    n_colors = plt.cm.tab10(np.linspace(0, 1, len(ns)))

    for n_idx, n in enumerate(ns):
        for method in methods:
            bpks = []
            valid_alphas = []
            for alpha in alphas:
                exp = grid.get((n, alpha))
                if exp is None:
                    continue
                match = next((r for r in exp["results"] if r["method"] == method), None)
                if match:
                    valid_alphas.append(alpha)
                    bpks.append(match["memory_bytes"] * 8 / exp["dataset"]["N"])

            if not bpks:
                continue

            marker, _ = _method_style(method)
            label = f"{METHOD_DISPLAY.get(method, method)} (N={_format_n(n)})"
            ax.plot(
                valid_alphas,
                bpks,
                marker=marker,
                markersize=5,
                color=n_colors[n_idx],
                linestyle="-" if "optimal" in method else "--",
                alpha=0.8 if "hash" not in method else 0.4,
                label=label,
            )

    ax.set_xlabel(r"$\alpha$")
    ax.set_ylabel("Memory (bits/key)")
    ax.set_title(f"Memory vs Alpha — {filter_type}")
    ax.legend(fontsize=7, loc="best", ncol=2)
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    return fig


def main():
    parser = argparse.ArgumentParser(
        description="Generate baseline comparison plots",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--filter-type",
        choices=["xor", "binary_fuse", "bloom"],
        default="binary_fuse",
        help="Filter type to plot",
    )
    args = parser.parse_args()

    plt.rcParams.update(
        {
            "font.size": 10,
            "axes.labelsize": 11,
            "axes.titlesize": 12,
            "legend.fontsize": 8,
            "xtick.labelsize": 9,
            "ytick.labelsize": 9,
            "lines.linewidth": 1.5,
        }
    )

    sweep_path = os.path.join(DATA_DIR, f"baselines_sweep_{args.filter_type}.json")
    sweep = load_json(sweep_path)

    if sweep is None:
        print(f"No sweep file found at {sweep_path}. Run run_baselines.py first.")
        return

    experiments = sweep["experiments"]

    print_table(
        f"Memory (bits/key) — {args.filter_type}",
        experiments,
        lambda r, ds: r["memory_bytes"] * 8 / ds["N"],
        fmt=".2f",
    )

    print_table(
        f"Avg Inference Time (ns) — {args.filter_type}",
        experiments,
        lambda r, ds: r["avg_inference_time_ns"],
        fmt=".0f",
    )

    print_table(
        f"Construction Time (s) — {args.filter_type}",
        experiments,
        lambda r, ds: r["construction_time_s"],
        fmt=".3f",
    )

    print("\n=== Inference vs Memory plots ===")
    fig = plot_inference_vs_memory_grid(experiments, args.filter_type)
    out = os.path.join(FIGURES_DIR, f"inference_vs_memory_{args.filter_type}.png")
    os.makedirs(FIGURES_DIR, exist_ok=True)
    fig.savefig(out, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  Saved: {out}")

    print("\n=== Memory vs Alpha plot ===")
    fig = plot_memory_vs_alpha(experiments, args.filter_type)
    out = os.path.join(FIGURES_DIR, f"memory_vs_alpha_{args.filter_type}.png")
    fig.savefig(out, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  Saved: {out}")


if __name__ == "__main__":
    main()
