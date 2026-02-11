"""
Optimal Filter Configuration Sweep.

For each alpha, find the filter configuration that maximizes savings,
then plot the envelope of optimal savings vs alpha.
"""

import argparse
from typing import Dict, List, Tuple

import matplotlib.pyplot as plt
import numpy as np
from bounds_validation import (
    DELTA,
    get_filter_params,
    theoretical_lower_bound,
    theoretical_upper_bound_tight,
)
from data_gen import gen_alpha_values, gen_keys
from measure import measure_csf

# Filter configurations to sweep
XOR_CONFIGS = [{"fingerprint_bits": b} for b in range(1, 9)]
BINARY_FUSE_CONFIGS = [{"fingerprint_bits": b} for b in range(1, 9)]
BLOOM_CONFIGS = [
    {"bloom_bits": b, "bloom_hashes": h} for b in range(1, 6) for h in range(1, 6)
]


def get_all_configs(filter_type: str) -> List[Dict]:
    """Get all configurations for a filter type."""
    if filter_type == "xor":
        return XOR_CONFIGS
    elif filter_type == "binary_fuse":
        return BINARY_FUSE_CONFIGS
    elif filter_type == "bloom":
        return BLOOM_CONFIGS
    else:
        raise ValueError(f"Unknown filter type: {filter_type}")


def get_theory_optimal_config(alpha: float, n_over_N: float) -> Tuple[str, Dict]:
    """
    Use theory to find optimal filter config (no empirical measurement needed).

    Returns:
        (filter_type, config) that maximizes lower bound
    """
    best_lower = float("-inf")
    best_filter = None
    best_config = None

    for filter_type in ["xor", "binary_fuse", "bloom"]:
        configs = get_all_configs(filter_type)
        for config in configs:
            if filter_type in ("xor", "binary_fuse"):
                b_eps, epsilon = get_filter_params(filter_type, config["fingerprint_bits"])
            else:
                b_eps, epsilon = get_filter_params(
                    "bloom",
                    bloom_bits=config["bloom_bits"],
                    bloom_hashes=config["bloom_hashes"],
                )

            lower = theoretical_lower_bound(alpha, epsilon, b_eps, n_over_N)
            if lower > best_lower:
                best_lower = lower
                best_filter = filter_type
                best_config = config

    return best_filter, best_config


def run_optimal_sweep(
    alphas: List[float],
    N: int,
    filter_type: str,
    minority_dist: str = "unique",
    seed: int = 42,
) -> List[Dict]:
    """
    For each alpha, find the optimal filter configuration.

    Returns list of dicts with:
        - alpha
        - best_savings (bits per key)
        - best_config
        - baseline_bytes
        - n (unique values)
    """
    configs = get_all_configs(filter_type)
    results = []

    for alpha in alphas:
        print(f"  alpha={alpha:.2f}", end="", flush=True)

        keys = gen_keys(N)
        values = gen_alpha_values(N, alpha, seed, minority_dist)

        # Baseline (no filter) - only compute once per alpha
        baseline = measure_csf(
            keys, values, "none", N, alpha, minority_dist=minority_dist
        )

        # Try all configs, find best
        best_savings = float("-inf")
        best_config = None

        for config in configs:
            if filter_type in ("xor", "binary_fuse"):
                filtered = measure_csf(
                    keys,
                    values,
                    filter_type,
                    N,
                    alpha,
                    fingerprint_bits=config["fingerprint_bits"],
                    minority_dist=minority_dist,
                )
            else:  # bloom
                filtered = measure_csf(
                    keys,
                    values,
                    "bloom",
                    N,
                    alpha,
                    bloom_bits_per_element=int(config["bloom_bits"]),
                    bloom_num_hashes=config["bloom_hashes"],
                    minority_dist=minority_dist,
                )

            savings = (baseline.total_bytes - filtered.total_bytes) * 8 / N
            if savings > best_savings:
                best_savings = savings
                best_config = config

        results.append(
            {
                "alpha": alpha,
                "best_savings": best_savings,
                "best_config": best_config,
                "baseline_bytes": baseline.total_bytes,
                "n": baseline.huffman_num_symbols,
            }
        )
        print(f" -> {best_savings:.3f} bpk ({best_config})")

    return results


def run_theory_guided_sweep(
    alphas: List[float],
    N: int,
    minority_dist: str = "unique",
    seed: int = 42,
) -> List[Dict]:
    """
    For each alpha, use THEORY to pick the optimal config, then measure empirically.

    This builds only ONE filter per alpha (the theory-optimal one).

    Returns list of dicts with:
        - alpha
        - theory_savings (empirical savings using theory's choice)
        - theory_config
        - theory_filter_type
        - lower_bound
        - upper_bound
        - n
    """
    results = []

    for alpha in alphas:
        print(f"  alpha={alpha:.2f}", end="", flush=True)

        keys = gen_keys(N)
        values = gen_alpha_values(N, alpha, seed, minority_dist)

        # Baseline (no filter)
        baseline = measure_csf(
            keys, values, "none", N, alpha, minority_dist=minority_dist
        )
        n_over_N = baseline.huffman_num_symbols / N

        # Theory picks the config (no grid search!)
        theory_filter, theory_config = get_theory_optimal_config(alpha, n_over_N)

        # Build only the theory-optimal filter
        if theory_filter in ("xor", "binary_fuse"):
            filtered = measure_csf(
                keys,
                values,
                theory_filter,
                N,
                alpha,
                fingerprint_bits=theory_config["fingerprint_bits"],
                minority_dist=minority_dist,
            )
            b_eps, epsilon = get_filter_params(
                theory_filter, theory_config["fingerprint_bits"]
            )
        else:  # bloom
            filtered = measure_csf(
                keys,
                values,
                "bloom",
                N,
                alpha,
                bloom_bits_per_element=int(theory_config["bloom_bits"]),
                bloom_num_hashes=theory_config["bloom_hashes"],
                minority_dist=minority_dist,
            )
            b_eps, epsilon = get_filter_params(
                "bloom",
                bloom_bits=theory_config["bloom_bits"],
                bloom_hashes=theory_config["bloom_hashes"],
            )

        theory_savings = (baseline.total_bytes - filtered.total_bytes) * 8 / N
        lower = theoretical_lower_bound(alpha, epsilon, b_eps, n_over_N)
        upper = theoretical_upper_bound_tight(alpha, epsilon, b_eps, n_over_N)

        results.append(
            {
                "alpha": alpha,
                "theory_savings": theory_savings,
                "theory_config": theory_config,
                "theory_filter_type": theory_filter,
                "lower_bound": lower,
                "upper_bound": upper,
                "n": baseline.huffman_num_symbols,
            }
        )
        config_str = f"{theory_filter}:{theory_config}"
        print(f" -> {theory_savings:.3f} bpk (theory: {config_str})")

    return results


def compute_theory_bounds(
    alpha: float, n_over_N: float, filter_type: str
) -> Tuple[float, float, Dict]:
    """
    Compute max lower bound and corresponding upper bound across all configs.

    Returns:
        Tuple of (max_lower_bound, corresponding_upper_bound, best_config)
    """
    configs = get_all_configs(filter_type)
    best_lower = float("-inf")
    best_upper = None
    best_config = None

    for config in configs:
        if filter_type in ("xor", "binary_fuse"):
            b_eps, epsilon = get_filter_params(filter_type, config["fingerprint_bits"])
        else:
            b_eps, epsilon = get_filter_params(
                "bloom",
                bloom_bits=config["bloom_bits"],
                bloom_hashes=config["bloom_hashes"],
            )

        lower = theoretical_lower_bound(alpha, epsilon, b_eps, n_over_N)
        upper = theoretical_upper_bound_tight(alpha, epsilon, b_eps, n_over_N)

        if lower > best_lower:
            best_lower = lower
            best_upper = upper
            best_config = config

    return best_lower, best_upper, best_config


def find_theory_crossover(
    alphas: List[float], n_over_N_values: List[float], filter_type: str
) -> float:
    """Find alpha where theory lower bound crosses zero."""
    for i in range(len(alphas) - 1):
        lower_i = compute_theory_bounds(alphas[i], n_over_N_values[i], filter_type)[0]
        lower_next = compute_theory_bounds(
            alphas[i + 1], n_over_N_values[i + 1], filter_type
        )[0]
        if lower_i < 0 and lower_next >= 0:
            return alphas[i] + (alphas[i + 1] - alphas[i]) * (-lower_i) / (
                lower_next - lower_i
            )
    return None


def compute_best_theory_bounds(
    alpha: float, n_over_N: float
) -> Tuple[float, float, str, Dict]:
    """
    Compute best lower/upper bounds across ALL filter types.

    Returns:
        (best_lower, corresponding_upper, best_filter_type, best_config)
    """
    best_lower = float("-inf")
    best_upper = None
    best_filter = None
    best_config = None

    for filter_type in ["xor", "binary_fuse", "bloom"]:
        lower, upper, config = compute_theory_bounds(alpha, n_over_N, filter_type)
        if lower > best_lower:
            best_lower = lower
            best_upper = upper
            best_filter = filter_type
            best_config = config

    return best_lower, best_upper, best_filter, best_config


def plot_optimal_sweep(
    results: List[Dict],
    filter_type: str,
    N: int,
    output_path: str = None,
    show_theory: bool = True,
) -> Tuple:
    """Plot optimal savings vs alpha with theory comparison."""
    alphas = [r["alpha"] for r in results]
    empirical = [r["best_savings"] for r in results]
    n_over_N_values = [r["n"] / N for r in results]

    # Compute theory bounds for each alpha (using best config at each point)
    if show_theory:
        theory_lower = []
        theory_upper = []
        for a, n_N in zip(alphas, n_over_N_values):
            lower, upper, _ = compute_theory_bounds(a, n_N, filter_type)
            theory_lower.append(lower)
            theory_upper.append(upper)

    fig, ax = plt.subplots(figsize=(12, 7))

    # Shaded region between bounds
    if show_theory:
        ax.fill_between(
            alphas,
            theory_lower,
            theory_upper,
            alpha=0.3,
            color="blue",
            label="Theoretical bounds",
        )
        ax.plot(alphas, theory_lower, "b--", linewidth=1.5, label="Lower bound")
        ax.plot(alphas, theory_upper, "b-", linewidth=1.5, label="Upper bound")

    # Empirical optimal
    ax.plot(
        alphas,
        empirical,
        "ro-",
        markersize=4,
        linewidth=2,
        label="Empirical optimal",
    )

    # Reference line
    ax.axhline(y=0, color="k", linestyle="-", alpha=0.5)

    # Find empirical crossover
    for i in range(len(empirical) - 1):
        if empirical[i] < 0 and empirical[i + 1] >= 0:
            emp_cross = alphas[i] + (alphas[i + 1] - alphas[i]) * (-empirical[i]) / (
                empirical[i + 1] - empirical[i]
            )
            ax.axvline(
                x=emp_cross,
                color="green",
                linestyle="--",
                alpha=0.7,
                label=f"Empirical crossover ({emp_cross:.3f})",
            )
            break

    # Find theoretical crossover
    if show_theory:
        theory_cross = find_theory_crossover(alphas, n_over_N_values, filter_type)
        if theory_cross is not None:
            ax.axvline(
                x=theory_cross,
                color="red",
                linestyle="--",
                alpha=0.7,
                label=f"Theory crossover ({theory_cross:.3f})",
            )

    filter_name = filter_type.replace("_", " ").title()
    ax.set_xlabel("α (frequency of majority value)", fontsize=12)
    ax.set_ylabel("Bits per key saved (positive = filter helps)", fontsize=12)
    ax.set_title(f"{filter_name}: Optimal Configuration Sweep\nN={N:,}")
    ax.legend(loc="upper left")
    ax.grid(True, alpha=0.3)
    ax.set_xlim(0.5, 1.0)

    plt.tight_layout()

    if output_path:
        plt.savefig(output_path, dpi=150, bbox_inches="tight")
        print(f"Saved: {output_path}")

    return fig, ax


def plot_all_filters_combined(
    results_by_filter: Dict[str, List[Dict]],
    theory_guided_results: List[Dict],
    N: int,
    output_path: str = None,
) -> Tuple:
    """
    Plot theory bounds and two empirical lines:
    1. Lower/Upper bounds from theory-optimal config (no grid search needed)
    2. Empirical (grid search): best savings found by trying all configs
    3. Empirical (theory-guided): savings using only theory's recommended config
    """
    fig, ax = plt.subplots(figsize=(12, 7))

    # Get alphas from theory-guided results
    alphas = [r["alpha"] for r in theory_guided_results]

    # Bounds come from theory-guided results (computed at theory's optimal config)
    theory_lower = [r["lower_bound"] for r in theory_guided_results]
    theory_upper = [r["upper_bound"] for r in theory_guided_results]
    theory_empirical = [r["theory_savings"] for r in theory_guided_results]

    # Best empirical at each alpha (max across all filters from grid search)
    best_empirical = []
    for i in range(len(alphas)):
        best = max(
            results_by_filter[ft][i]["best_savings"] for ft in results_by_filter
        )
        best_empirical.append(best)

    # Shaded region between bounds
    ax.fill_between(
        alphas,
        theory_lower,
        theory_upper,
        alpha=0.3,
        color="blue",
        label="Theoretical bounds (at theory ε*)",
    )

    # Bound lines
    ax.plot(alphas, theory_upper, "b-", linewidth=1.5, label="Upper bound")
    ax.plot(alphas, theory_lower, "b--", linewidth=1.5, label="Lower bound")

    # Empirical lines
    ax.plot(
        alphas,
        best_empirical,
        "ro-",
        markersize=4,
        linewidth=2,
        label="Empirical (grid search)",
    )
    ax.plot(
        alphas,
        theory_empirical,
        "g^-",
        markersize=4,
        linewidth=2,
        label="Empirical (theory-guided)",
    )

    # Reference line
    ax.axhline(y=0, color="k", linestyle="-", alpha=0.5)

    # Find crossovers
    for i in range(len(best_empirical) - 1):
        if best_empirical[i] < 0 and best_empirical[i + 1] >= 0:
            emp_cross = alphas[i] + (alphas[i + 1] - alphas[i]) * (
                -best_empirical[i]
            ) / (best_empirical[i + 1] - best_empirical[i])
            ax.axvline(
                x=emp_cross,
                color="red",
                linestyle=":",
                alpha=0.7,
                label=f"Grid search crossover ({emp_cross:.3f})",
            )
            break

    for i in range(len(theory_empirical) - 1):
        if theory_empirical[i] < 0 and theory_empirical[i + 1] >= 0:
            theory_emp_cross = alphas[i] + (alphas[i + 1] - alphas[i]) * (
                -theory_empirical[i]
            ) / (theory_empirical[i + 1] - theory_empirical[i])
            ax.axvline(
                x=theory_emp_cross,
                color="green",
                linestyle=":",
                alpha=0.7,
                label=f"Theory-guided crossover ({theory_emp_cross:.3f})",
            )
            break

    for i in range(len(theory_lower) - 1):
        if theory_lower[i] < 0 and theory_lower[i + 1] >= 0:
            theory_cross = alphas[i] + (alphas[i + 1] - alphas[i]) * (
                -theory_lower[i]
            ) / (theory_lower[i + 1] - theory_lower[i])
            ax.axvline(
                x=theory_cross,
                color="blue",
                linestyle=":",
                alpha=0.7,
                label=f"Lower bound crossover ({theory_cross:.3f})",
            )
            break

    ax.set_xlabel("α (frequency of majority value)", fontsize=12)
    ax.set_ylabel("Bits per key saved (positive = filter helps)", fontsize=12)
    ax.set_title(f"Optimal Filter: Theory vs Empirical\nN={N:,}")
    ax.legend(loc="upper left")
    ax.grid(True, alpha=0.3)
    ax.set_xlim(0.5, 1.0)

    plt.tight_layout()

    if output_path:
        plt.savefig(output_path, dpi=150, bbox_inches="tight")
        print(f"Saved: {output_path}")

    return fig, ax


def main():
    parser = argparse.ArgumentParser(
        description="Sweep filter configurations to find optimal savings at each alpha"
    )
    parser.add_argument(
        "--filter",
        choices=["xor", "binary_fuse", "bloom", "all"],
        required=True,
        help="Filter type to sweep (or 'all' for combined plot)",
    )
    parser.add_argument(
        "--N",
        type=int,
        default=100_000,
        help="Number of keys (default: 100,000)",
    )
    parser.add_argument(
        "--alpha-start",
        type=float,
        default=0.50,
        help="Starting alpha value (default: 0.50)",
    )
    parser.add_argument(
        "--alpha-end",
        type=float,
        default=0.99,
        help="Ending alpha value (default: 0.99)",
    )
    parser.add_argument(
        "--alpha-step",
        type=float,
        default=0.01,
        help="Alpha step size (default: 0.01)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=str,
        default=None,
        help="Output file path for figure",
    )
    parser.add_argument(
        "--show",
        action="store_true",
        help="Display the plot interactively",
    )

    args = parser.parse_args()
    alphas = list(np.arange(args.alpha_start, args.alpha_end + 0.001, args.alpha_step))

    if args.filter == "all":
        # Run grid search for all filter types
        results_by_filter = {}
        for ft in ["xor", "binary_fuse", "bloom"]:
            print(f"Running grid search for {ft}...")
            results_by_filter[ft] = run_optimal_sweep(alphas, args.N, ft)

        # Run theory-guided sweep (builds only ONE filter per alpha)
        print("Running theory-guided sweep...")
        theory_guided_results = run_theory_guided_sweep(alphas, args.N)

        plot_all_filters_combined(
            results_by_filter, theory_guided_results, args.N, args.output
        )
    else:
        results = run_optimal_sweep(alphas, args.N, args.filter)
        plot_optimal_sweep(results, args.filter, args.N, args.output)

    if args.show:
        plt.show()


if __name__ == "__main__":
    main()
