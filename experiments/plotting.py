"""Visualization utilities for filter optimization experiments."""

import json
import os
from collections import defaultdict
from typing import Optional

import matplotlib.pyplot as plt
import numpy as np

from measure import ExperimentResult


def load_results(filepath: str) -> list[ExperimentResult]:
    """Load results from JSON file."""
    with open(filepath, "r") as f:
        data = json.load(f)
    return [ExperimentResult.from_dict(r) for r in data["results"]]


def plot_size_vs_alpha(
    results: list[ExperimentResult],
    n: int,
    minority_dist: str = None,
    output_path: Optional[str] = None,
    show: bool = True,
):
    """
    Plot total_bytes vs alpha for each filter type.

    Shows where each filter becomes beneficial compared to no filter.

    Args:
        results: List of ExperimentResult
        n: N value to filter on
        minority_dist: Filter to specific minority distribution (or None for first found)
        output_path: Optional path to save the plot
        show: Whether to display the plot
    """
    # Filter by n and minority_dist
    if minority_dist:
        filtered = [r for r in results if r.n == n and r.minority_dist == minority_dist]
    else:
        # Use first minority_dist found
        dists = set(r.minority_dist for r in results if r.n == n)
        if dists:
            minority_dist = sorted(dists)[0]
            filtered = [r for r in results if r.n == n and r.minority_dist == minority_dist]
        else:
            filtered = [r for r in results if r.n == n]

    # Group by filter config
    by_config = defaultdict(list)
    for r in filtered:
        if r.filter_type == "none":
            key = "none"
        elif r.filter_type in ("xor", "binary_fuse"):
            key = f"{r.filter_type}_{r.fingerprint_bits}"
        elif r.filter_type == "bloom":
            # Include both bits_per_element (derived from bloom_size) and num_hashes
            # bloom_size is in bits, we estimate bits_per_element from it
            key = f"bloom_{r.bloom_size}b_{r.bloom_num_hashes}h"
        else:
            continue
        by_config[key].append(r)

    plt.figure(figsize=(12, 8))

    # Sort configs for consistent ordering
    for config_key in sorted(by_config.keys()):
        config_results = sorted(by_config[config_key], key=lambda r: r.alpha)
        alphas = [r.alpha for r in config_results]
        sizes = [r.total_bytes for r in config_results]

        # Style based on filter type
        if config_key == "none":
            plt.plot(alphas, sizes, "k-", linewidth=2, label="no filter", marker="o")
        elif config_key.startswith("xor"):
            plt.plot(alphas, sizes, "--", label=config_key, marker="s", alpha=0.7)
        elif config_key.startswith("binary_fuse"):
            plt.plot(alphas, sizes, "-.", label=config_key, marker="^", alpha=0.7)
        elif config_key.startswith("bloom"):
            plt.plot(alphas, sizes, ":", label=config_key, marker="d", alpha=0.7)

    plt.xlabel("Alpha (frequency of most common element)")
    plt.ylabel("Total Size (bytes)")
    dist_str = f", {minority_dist}" if minority_dist else ""
    plt.title(f"CSF Size vs Alpha (N={n:,}{dist_str})")
    plt.legend(bbox_to_anchor=(1.05, 1), loc="upper left")
    plt.grid(True, alpha=0.3)
    plt.tight_layout()

    if output_path:
        os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
        plt.savefig(output_path, dpi=150, bbox_inches="tight")
        print(f"Saved: {output_path}")

    if show:
        plt.show()
    else:
        plt.close()


def plot_size_vs_alpha_by_filter_type(
    results: list[ExperimentResult],
    n: int,
    filter_type: str,
    output_path: Optional[str] = None,
    show: bool = True,
):
    """
    Plot total_bytes vs alpha for different parameter values of a single filter type.

    Args:
        results: List of ExperimentResult
        n: N value to filter on
        filter_type: 'xor', 'binary_fuse', or 'bloom'
        output_path: Optional path to save the plot
        show: Whether to display the plot
    """
    filtered = [r for r in results if r.n == n and r.filter_type in (filter_type, "none")]

    # Also get no-filter baseline
    no_filter = [r for r in filtered if r.filter_type == "none"]
    with_filter = [r for r in filtered if r.filter_type == filter_type]

    plt.figure(figsize=(10, 6))

    # Plot no-filter baseline
    if no_filter:
        no_filter = sorted(no_filter, key=lambda r: r.alpha)
        plt.plot(
            [r.alpha for r in no_filter],
            [r.total_bytes for r in no_filter],
            "k-",
            linewidth=2,
            label="no filter",
            marker="o",
        )

    # Group by parameter value
    by_param = defaultdict(list)
    for r in with_filter:
        if filter_type in ("xor", "binary_fuse"):
            key = r.fingerprint_bits
        else:
            key = f"{r.bloom_num_hashes}h"
        by_param[key].append(r)

    colors = plt.cm.viridis(np.linspace(0, 0.8, len(by_param)))

    for (param, param_results), color in zip(sorted(by_param.items()), colors):
        param_results = sorted(param_results, key=lambda r: r.alpha)
        alphas = [r.alpha for r in param_results]
        sizes = [r.total_bytes for r in param_results]

        if filter_type in ("xor", "binary_fuse"):
            label = f"{param} bits"
        else:
            label = param

        plt.plot(alphas, sizes, "-", color=color, label=label, marker="s", alpha=0.8)

    plt.xlabel("Alpha (frequency of most common element)")
    plt.ylabel("Total Size (bytes)")
    plt.title(f"{filter_type.replace('_', ' ').title()} Filter: Size vs Alpha (N={n:,})")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()

    if output_path:
        os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
        plt.savefig(output_path, dpi=150, bbox_inches="tight")
        print(f"Saved: {output_path}")

    if show:
        plt.show()
    else:
        plt.close()


def plot_optimal_filter_heatmap(
    results: list[ExperimentResult],
    output_path: Optional[str] = None,
    show: bool = True,
):
    """
    Heatmap showing optimal filter type for each (N, alpha) combination.

    Args:
        results: List of ExperimentResult
        output_path: Optional path to save the plot
        show: Whether to display the plot
    """
    # Find unique N and alpha values
    n_values = sorted(set(r.n for r in results))
    alpha_values = sorted(set(r.alpha for r in results))

    # Find optimal filter for each (n, alpha)
    optimal = {}
    for n in n_values:
        for alpha in alpha_values:
            candidates = [r for r in results if r.n == n and abs(r.alpha - alpha) < 0.001]
            if candidates:
                best = min(candidates, key=lambda r: r.total_bytes)
                optimal[(n, alpha)] = best.filter_type

    # Create heatmap data
    filter_types = ["none", "xor", "binary_fuse", "bloom"]
    type_to_num = {t: i for i, t in enumerate(filter_types)}

    data = np.zeros((len(n_values), len(alpha_values)))
    for i, n in enumerate(n_values):
        for j, alpha in enumerate(alpha_values):
            ft = optimal.get((n, alpha), "none")
            data[i, j] = type_to_num.get(ft, 0)

    plt.figure(figsize=(12, 6))
    im = plt.imshow(data, aspect="auto", cmap="Set2")

    plt.xticks(range(len(alpha_values)), [f"{a:.2f}" for a in alpha_values])
    plt.yticks(range(len(n_values)), [f"{n:,}" for n in n_values])
    plt.xlabel("Alpha")
    plt.ylabel("N")
    plt.title("Optimal Filter Type by (N, Alpha)")

    # Add colorbar with filter type labels
    cbar = plt.colorbar(im, ticks=range(len(filter_types)))
    cbar.ax.set_yticklabels(filter_types)

    plt.tight_layout()

    if output_path:
        os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
        plt.savefig(output_path, dpi=150, bbox_inches="tight")
        print(f"Saved: {output_path}")

    if show:
        plt.show()
    else:
        plt.close()


def plot_fingerprint_bits_tradeoff(
    results: list[ExperimentResult],
    n: int,
    alpha: float,
    output_path: Optional[str] = None,
    show: bool = True,
):
    """
    Plot total_bytes vs fingerprint_bits for XOR and BinaryFuse.

    Args:
        results: List of ExperimentResult
        n: N value to filter on
        alpha: Alpha value to filter on
        output_path: Optional path to save the plot
        show: Whether to display the plot
    """
    filtered = [
        r for r in results
        if r.n == n and abs(r.alpha - alpha) < 0.001
        and r.filter_type in ("xor", "binary_fuse")
    ]

    # Group by filter type
    xor_results = sorted(
        [r for r in filtered if r.filter_type == "xor"],
        key=lambda r: r.fingerprint_bits
    )
    bf_results = sorted(
        [r for r in filtered if r.filter_type == "binary_fuse"],
        key=lambda r: r.fingerprint_bits
    )

    # Get no-filter baseline
    no_filter = [
        r for r in results
        if r.n == n and abs(r.alpha - alpha) < 0.001 and r.filter_type == "none"
    ]
    baseline = no_filter[0].total_bytes if no_filter else None

    plt.figure(figsize=(10, 6))

    if xor_results:
        plt.plot(
            [r.fingerprint_bits for r in xor_results],
            [r.total_bytes for r in xor_results],
            "o-",
            label="XOR",
            markersize=8,
        )

    if bf_results:
        plt.plot(
            [r.fingerprint_bits for r in bf_results],
            [r.total_bytes for r in bf_results],
            "s-",
            label="Binary Fuse",
            markersize=8,
        )

    if baseline:
        plt.axhline(y=baseline, color="k", linestyle="--", label="No filter", alpha=0.7)

    plt.xlabel("Fingerprint Bits")
    plt.ylabel("Total Size (bytes)")
    plt.title(f"Size vs Fingerprint Bits (N={n:,}, alpha={alpha:.2f})")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()

    if output_path:
        os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
        plt.savefig(output_path, dpi=150, bbox_inches="tight")
        print(f"Saved: {output_path}")

    if show:
        plt.show()
    else:
        plt.close()


def plot_size_breakdown(
    results: list[ExperimentResult],
    n: int,
    alpha: float,
    output_path: Optional[str] = None,
    show: bool = True,
):
    """
    Stacked bar chart showing size breakdown (solution, filter, metadata).

    Args:
        results: List of ExperimentResult
        n: N value to filter on
        alpha: Alpha value to filter on
        output_path: Optional path to save the plot
        show: Whether to display the plot
    """
    filtered = [
        r for r in results
        if r.n == n and abs(r.alpha - alpha) < 0.001
    ]

    # Group and sort by total size
    filtered = sorted(filtered, key=lambda r: r.total_bytes)

    # Create labels
    labels = []
    for r in filtered:
        if r.filter_type == "none":
            labels.append("none")
        elif r.filter_type in ("xor", "binary_fuse"):
            labels.append(f"{r.filter_type}_{r.fingerprint_bits}")
        else:
            labels.append(f"bloom_{r.bloom_num_hashes}h")

    solution = [r.solution_bytes for r in filtered]
    filter_b = [r.filter_bytes for r in filtered]
    metadata = [r.metadata_bytes for r in filtered]

    x = np.arange(len(labels))
    width = 0.8

    plt.figure(figsize=(14, 6))
    plt.bar(x, solution, width, label="Solution")
    plt.bar(x, filter_b, width, bottom=solution, label="Filter")
    plt.bar(x, metadata, width, bottom=np.array(solution) + np.array(filter_b), label="Metadata")

    plt.xlabel("Filter Configuration")
    plt.ylabel("Size (bytes)")
    plt.title(f"Size Breakdown (N={n:,}, alpha={alpha:.2f})")
    plt.xticks(x, labels, rotation=45, ha="right")
    plt.legend()
    plt.tight_layout()

    if output_path:
        os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
        plt.savefig(output_path, dpi=150, bbox_inches="tight")
        print(f"Saved: {output_path}")

    if show:
        plt.show()
    else:
        plt.close()


def analyze_crossover_points(results: list[ExperimentResult]) -> dict:
    """
    Find the crossover alpha where each filter type becomes beneficial vs no filter.

    For each (N, minority_dist, filter_type), find the lowest alpha where
    the best configuration of that filter beats no filter.

    Returns:
        Dict with crossover analysis by distribution, N, and filter type
    """
    n_values = sorted(set(r.n for r in results))
    alpha_values = sorted(set(r.alpha for r in results))
    minority_dists = sorted(set(r.minority_dist for r in results))
    filter_types = ["xor", "binary_fuse", "bloom"]

    analysis = {}

    for minority_dist in minority_dists:
        analysis[minority_dist] = {}

        for n in n_values:
            analysis[minority_dist][str(n)] = {}

            # Get no-filter baseline for each alpha
            no_filter_by_alpha = {}
            for r in results:
                if (r.n == n and r.minority_dist == minority_dist
                        and r.filter_type == "none"):
                    no_filter_by_alpha[r.alpha] = r.total_bytes

            for filter_type in filter_types:
                # For each alpha, find best config of this filter type
                best_by_alpha = {}
                for alpha in alpha_values:
                    candidates = [
                        r for r in results
                        if (r.n == n and r.minority_dist == minority_dist
                            and r.filter_type == filter_type
                            and abs(r.alpha - alpha) < 0.001)
                    ]
                    if candidates:
                        best = min(candidates, key=lambda r: r.total_bytes)
                        best_by_alpha[alpha] = {
                            "total_bytes": best.total_bytes,
                            "fingerprint_bits": best.fingerprint_bits,
                            "bloom_size": best.bloom_size,
                            "bloom_num_hashes": best.bloom_num_hashes,
                        }

                # Find crossover point
                crossover_alpha = None
                crossover_config = None
                for alpha in alpha_values:
                    if alpha in best_by_alpha and alpha in no_filter_by_alpha:
                        if best_by_alpha[alpha]["total_bytes"] < no_filter_by_alpha[alpha]:
                            crossover_alpha = alpha
                            crossover_config = best_by_alpha[alpha]
                            break

                # Calculate savings at highest alpha tested
                max_alpha = max(alpha_values)
                savings_at_max = None
                if max_alpha in best_by_alpha and max_alpha in no_filter_by_alpha:
                    no_filter_size = no_filter_by_alpha[max_alpha]
                    filter_size = best_by_alpha[max_alpha]["total_bytes"]
                    savings_at_max = {
                        "alpha": max_alpha,
                        "no_filter_bytes": no_filter_size,
                        "filter_bytes": filter_size,
                        "savings_bytes": no_filter_size - filter_size,
                        "savings_percent": round(100 * (no_filter_size - filter_size) / no_filter_size, 1),
                    }

                analysis[minority_dist][str(n)][filter_type] = {
                    "crossover_alpha": crossover_alpha,
                    "crossover_config": crossover_config,
                    "savings_at_max_alpha": savings_at_max,
                }

    return analysis


def generate_summary_report(
    results: list[ExperimentResult],
    output_dir: str,
    show: bool = False,
):
    """
    Generate all plots and a summary JSON with optimal configs.

    Args:
        results: List of ExperimentResult
        output_dir: Directory to save outputs
        show: Whether to display plots
    """
    os.makedirs(output_dir, exist_ok=True)

    n_values = sorted(set(r.n for r in results))
    alpha_values = sorted(set(r.alpha for r in results))
    minority_dists = sorted(set(r.minority_dist for r in results))

    # Generate plots for each (N, minority_dist) combination
    for minority_dist in minority_dists:
        dist_suffix = f"_{minority_dist}" if len(minority_dists) > 1 else ""
        dist_results = [r for r in results if r.minority_dist == minority_dist]

        for n in n_values:
            plot_size_vs_alpha(
                dist_results, n, minority_dist=minority_dist,
                output_path=f"{output_dir}/size_vs_alpha_n{n}{dist_suffix}.png",
                show=show,
            )

            for filter_type in ["xor", "binary_fuse"]:
                plot_size_vs_alpha_by_filter_type(
                    dist_results, n, filter_type,
                    output_path=f"{output_dir}/{filter_type}_params_n{n}{dist_suffix}.png",
                    show=show,
                )

        # Generate fingerprint bits tradeoff for high alpha
        for n in n_values:
            for alpha in [0.90, 0.95]:
                if alpha in alpha_values:
                    plot_fingerprint_bits_tradeoff(
                        dist_results, n, alpha,
                        output_path=f"{output_dir}/fingerprint_tradeoff_n{n}_a{int(alpha*100)}{dist_suffix}.png",
                        show=show,
                    )

                    plot_size_breakdown(
                        dist_results, n, alpha,
                        output_path=f"{output_dir}/size_breakdown_n{n}_a{int(alpha*100)}{dist_suffix}.png",
                        show=show,
                    )

    # Generate heatmap (use first minority_dist for now)
    if minority_dists:
        first_dist_results = [r for r in results if r.minority_dist == minority_dists[0]]
        plot_optimal_filter_heatmap(
            first_dist_results,
            output_path=f"{output_dir}/optimal_filter_heatmap.png",
            show=show,
        )

    # Generate summary JSON
    summary = {
        "n_values": n_values,
        "alpha_values": alpha_values,
        "minority_dists": minority_dists,
        "optimal_configs": {},
    }

    for minority_dist in minority_dists:
        dist_key = minority_dist
        summary["optimal_configs"][dist_key] = {}

        for n in n_values:
            summary["optimal_configs"][dist_key][str(n)] = {}
            for alpha in alpha_values:
                candidates = [
                    r for r in results
                    if r.n == n and abs(r.alpha - alpha) < 0.001
                    and r.minority_dist == minority_dist
                ]
                if candidates:
                    best = min(candidates, key=lambda r: r.total_bytes)
                    summary["optimal_configs"][dist_key][str(n)][str(alpha)] = {
                        "filter_type": best.filter_type,
                        "fingerprint_bits": best.fingerprint_bits,
                        "bloom_num_hashes": best.bloom_num_hashes,
                        "total_bytes": best.total_bytes,
                        "bits_per_key": best.bits_per_key,
                    }

    # Add crossover analysis
    summary["crossover_analysis"] = analyze_crossover_points(results)

    summary_path = f"{output_dir}/summary.json"
    with open(summary_path, "w") as f:
        json.dump(summary, f, indent=2)
    print(f"Saved: {summary_path}")


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Generate plots from experiment results")
    parser.add_argument("input", help="Input JSON file with results")
    parser.add_argument("-o", "--output-dir", default="experiments/results/plots")
    parser.add_argument("--show", action="store_true", help="Display plots")

    args = parser.parse_args()

    results = load_results(args.input)
    generate_summary_report(results, args.output_dir, show=args.show)
