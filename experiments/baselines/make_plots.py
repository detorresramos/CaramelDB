"""Plot generation for baseline comparisons.

Reads JSON results from figures/data/ and generates:
1. Inference time vs memory scatter plots
2. Construction time comparison tables

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
    "csf_filter_optimal_xor": "o",
    "csf_filter_optimal_binary_fuse": "o",
    "csf_filter_optimal_bloom": "o",
    "csf_filter_shibuya_xor": "^",
    "csf_filter_shibuya_binary_fuse": "^",
    "csf_filter_shibuya_bloom": "^",
}

METHOD_COLORS = {
    "hash_table": "tab:gray",
    "csf_filter_optimal_xor": "tab:blue",
    "csf_filter_optimal_binary_fuse": "tab:blue",
    "csf_filter_optimal_bloom": "tab:blue",
    "csf_filter_shibuya_xor": "tab:orange",
    "csf_filter_shibuya_binary_fuse": "tab:orange",
    "csf_filter_shibuya_bloom": "tab:orange",
}


def load_json(path):
    if not os.path.exists(path):
        return None
    with open(path) as f:
        return json.load(f)


def plot_inference_vs_memory(data, ax=None):
    """Scatter plot: inference time (ns) vs memory (bytes/key)."""
    if ax is None:
        fig, ax = plt.subplots(figsize=(8, 6))
    else:
        fig = ax.figure

    dataset = data["dataset"]
    N = dataset["N"]

    for r in data["results"]:
        method = r["method"]
        memory_per_key = r["memory_bytes"] / N
        inference_ns = r["avg_inference_time_ns"]

        label = METHOD_DISPLAY.get(method, method)
        marker = METHOD_MARKERS.get(method, "o")
        color = METHOD_COLORS.get(method, "tab:blue")

        ax.scatter(memory_per_key, inference_ns, s=100, marker=marker,
                   color=color, label=label, zorder=5)
        ax.annotate(label, (memory_per_key, inference_ns),
                    textcoords="offset points", xytext=(8, 4), fontsize=8)

    ax.set_xlabel("Memory (bytes/key)")
    ax.set_ylabel("Avg inference time (ns)")
    ax.set_title(
        f"Inference vs Memory — "
        f"N={N:,}, α={dataset['alpha']}, dist={dataset['minority_dist']}"
    )
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=8, loc="best")

    return fig, ax


def plot_construction_time_table(experiments, ax=None):
    """Render construction time comparison as a table figure."""
    if not experiments:
        return None, None

    if ax is None:
        fig, ax = plt.subplots(figsize=(10, 3))
    else:
        fig = ax.figure

    # Collect all methods and dataset labels
    methods = []
    for exp in experiments:
        for r in exp["results"]:
            if r["method"] not in methods:
                methods.append(r["method"])

    col_labels = []
    for exp in experiments:
        ds = exp["dataset"]
        col_labels.append(f"α={ds['alpha']}\n{ds['minority_dist']}")

    cell_text = []
    row_labels = []
    for method in methods:
        row_labels.append(METHOD_DISPLAY.get(method, method))
        row = []
        for exp in experiments:
            match = next((r for r in exp["results"] if r["method"] == method), None)
            if match:
                row.append(f"{match['construction_time_s']:.3f}s")
            else:
                row.append("—")
        cell_text.append(row)

    ax.axis("off")
    table = ax.table(
        cellText=cell_text,
        rowLabels=row_labels,
        colLabels=col_labels,
        cellLoc="center",
        loc="center",
    )
    table.auto_set_font_size(False)
    table.set_fontsize(9)
    table.scale(1.2, 1.5)
    ax.set_title("Construction Time Comparison", fontsize=12, pad=20)

    return fig, ax


def main():
    parser = argparse.ArgumentParser(
        description="Generate baseline comparison plots",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--filter-type", choices=["xor", "binary_fuse", "bloom"],
                        default="binary_fuse", help="Filter type to plot")
    args = parser.parse_args()

    plt.rcParams.update({
        "font.size": 10,
        "axes.labelsize": 11,
        "axes.titlesize": 12,
        "legend.fontsize": 8,
        "xtick.labelsize": 9,
        "ytick.labelsize": 9,
        "lines.linewidth": 1.5,
    })

    # Load combined results
    combined_path = os.path.join(DATA_DIR, f"baselines_combined_{args.filter_type}.json")
    combined = load_json(combined_path)

    if combined is None:
        # Fall back to loading individual files
        print(f"No combined file found at {combined_path}, loading individual files...")
        import glob
        pattern = os.path.join(DATA_DIR, f"baselines_a*_{args.filter_type}.json")
        files = sorted(glob.glob(pattern))
        if not files:
            print("No data files found. Run run_baselines.py first.")
            return
        experiments = [load_json(f) for f in files]
    else:
        experiments = combined["experiments"]

    # 1. Inference vs Memory scatter plots (one per experiment)
    print("=== Inference vs Memory plots ===")
    for exp in experiments:
        ds = exp["dataset"]
        fig, ax = plt.subplots(figsize=(8, 6))
        plot_inference_vs_memory(exp, ax)
        plt.tight_layout()

        filename = f"inference_vs_memory_n{ds['N']}_a{ds['alpha']}_{ds['minority_dist']}_{args.filter_type}.png"
        out = os.path.join(FIGURES_DIR, filename)
        fig.savefig(out, dpi=300, bbox_inches="tight")
        plt.close(fig)
        print(f"  Saved: {out}")

    # 2. Construction time table
    print("\n=== Construction time table ===")
    fig, ax = plt.subplots(figsize=(10, 3))
    plot_construction_time_table(experiments, ax)
    plt.tight_layout()

    N = experiments[0]["dataset"]["N"]
    out = os.path.join(FIGURES_DIR, f"construction_time_table_n{N}_{args.filter_type}.png")
    fig.savefig(out, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  Saved: {out}")

    # Also print table to stdout
    print("\nConstruction Time (seconds):")
    print("-" * 70)
    header = "Method".ljust(35)
    for exp in experiments:
        ds = exp["dataset"]
        header += f"  α={ds['alpha']} {ds['minority_dist'][:8]:>8}"
    print(header)
    print("-" * 70)

    methods = []
    for exp in experiments:
        for r in exp["results"]:
            if r["method"] not in methods:
                methods.append(r["method"])

    for method in methods:
        row = METHOD_DISPLAY.get(method, method).ljust(35)
        for exp in experiments:
            match = next((r for r in exp["results"] if r["method"] == method), None)
            if match:
                row += f"  {match['construction_time_s']:>14.3f}"
            else:
                row += f"  {'—':>14}"
        print(row)


if __name__ == "__main__":
    main()
