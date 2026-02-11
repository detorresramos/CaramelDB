"""
Epsilon Sweep at Fixed Alpha.

For a fixed alpha, sweep over epsilon values (filter configurations) and compare
where theoretical epsilon* vs empirical epsilon* land.
"""

import argparse
from typing import Dict, List, Tuple

import matplotlib.pyplot as plt
import numpy as np
from bounds_validation import (
    get_filter_params,
    theoretical_lower_bound,
    theoretical_upper_bound_tight,
)
from data_gen import gen_alpha_values, gen_keys
from measure import measure_csf


def run_epsilon_sweep(
    alpha: float,
    N: int,
    filter_type: str = "binary_fuse",
    max_fingerprint_bits: int = 8,
    minority_dist: str = "unique",
    seed: int = 42,
) -> List[Dict]:
    """
    Sweep over fingerprint_bits (1 to max) at fixed alpha.

    Returns list of dicts with:
        - fingerprint_bits
        - epsilon
        - b_eps (filter bits per key)
        - empirical_savings
        - lower_bound
        - upper_bound
    """
    keys = gen_keys(N)
    values = gen_alpha_values(N, alpha, seed, minority_dist)

    # Baseline (no filter)
    baseline = measure_csf(keys, values, "none", N, alpha, minority_dist=minority_dist)
    n_over_N = baseline.huffman_num_symbols / N

    results = []
    for bits in range(1, max_fingerprint_bits + 1):
        # Get theoretical params
        b_eps, epsilon = get_filter_params(filter_type, bits)

        # Build filter and measure
        filtered = measure_csf(
            keys,
            values,
            filter_type,
            N,
            alpha,
            fingerprint_bits=bits,
            minority_dist=minority_dist,
        )

        empirical_savings = (baseline.total_bytes - filtered.total_bytes) * 8 / N
        lower = theoretical_lower_bound(alpha, epsilon, b_eps, n_over_N)
        upper = theoretical_upper_bound_tight(alpha, epsilon, b_eps, n_over_N)

        results.append(
            {
                "fingerprint_bits": bits,
                "epsilon": epsilon,
                "b_eps": b_eps,
                "empirical_savings": empirical_savings,
                "lower_bound": lower,
                "upper_bound": upper,
            }
        )

    return results


def find_optimal_epsilon(results: List[Dict]) -> Tuple[Dict, Dict]:
    """
    Find epsilon*_theory and epsilon*_empirical from sweep results.

    Returns:
        (theory_optimal, empirical_optimal) dicts
    """
    theory_opt = max(results, key=lambda r: r["lower_bound"])
    empirical_opt = max(results, key=lambda r: r["empirical_savings"])
    return theory_opt, empirical_opt


def plot_epsilon_sweep(
    results: List[Dict],
    alpha: float,
    filter_type: str,
    N: int,
    output_path: str = None,
) -> Tuple:
    """Plot savings vs fingerprint_bits with theory comparison."""
    bits = [r["fingerprint_bits"] for r in results]
    epsilons = [r["epsilon"] for r in results]
    empirical = [r["empirical_savings"] for r in results]
    lower = [r["lower_bound"] for r in results]
    upper = [r["upper_bound"] for r in results]

    theory_opt, empirical_opt = find_optimal_epsilon(results)

    fig, ax = plt.subplots(figsize=(12, 7))

    # Shaded region
    ax.fill_between(
        bits, lower, upper, alpha=0.3, color="blue", label="Theoretical bounds"
    )

    # Lines
    ax.plot(bits, upper, "b-", linewidth=1.5, label="Upper bound")
    ax.plot(bits, lower, "b--", linewidth=1.5, label="Lower bound")
    ax.plot(bits, empirical, "ro-", markersize=8, linewidth=2, label="Empirical")

    # Mark optima
    ax.axvline(
        x=theory_opt["fingerprint_bits"],
        color="blue",
        linestyle=":",
        alpha=0.8,
        linewidth=2,
        label=f"epsilon*_theory (bits={theory_opt['fingerprint_bits']})",
    )
    ax.axvline(
        x=empirical_opt["fingerprint_bits"],
        color="red",
        linestyle=":",
        alpha=0.8,
        linewidth=2,
        label=f"epsilon*_empirical (bits={empirical_opt['fingerprint_bits']})",
    )

    # Reference
    ax.axhline(y=0, color="k", linestyle="-", alpha=0.5)

    # Secondary x-axis for epsilon
    ax2 = ax.twiny()
    ax2.set_xlim(ax.get_xlim())
    ax2.set_xticks(bits)
    ax2.set_xticklabels([f"{e:.3f}" for e in epsilons])
    ax2.set_xlabel("epsilon (false positive rate)", fontsize=10)

    filter_name = filter_type.replace("_", " ").title()
    ax.set_xlabel("Fingerprint bits", fontsize=12)
    ax.set_ylabel("Bits per key saved", fontsize=12)
    ax.set_title(f"{filter_name}: Epsilon Sweep at alpha={alpha}\nN={N:,}")
    ax.legend(loc="best")
    ax.grid(True, alpha=0.3)
    ax.set_xticks(bits)

    plt.tight_layout()
    if output_path:
        plt.savefig(output_path, dpi=150, bbox_inches="tight")
        print(f"Saved: {output_path}")

    return fig, ax


def main():
    parser = argparse.ArgumentParser(
        description="Sweep over epsilon values at fixed alpha to find optimal filter config"
    )
    parser.add_argument("--alpha", type=float, required=True, help="Fixed alpha value")
    parser.add_argument(
        "--filter",
        choices=["xor", "binary_fuse"],
        default="binary_fuse",
        help="Filter type (default: binary_fuse)",
    )
    parser.add_argument(
        "--N", type=int, default=100_000, help="Number of keys (default: 100,000)"
    )
    parser.add_argument(
        "--max-bits",
        type=int,
        default=8,
        help="Maximum fingerprint bits to sweep (default: 8)",
    )
    parser.add_argument(
        "-o", "--output", type=str, default=None, help="Output file path for figure"
    )
    parser.add_argument(
        "--show", action="store_true", help="Display the plot interactively"
    )

    args = parser.parse_args()

    print(f"Running epsilon sweep for alpha={args.alpha}, filter={args.filter}")
    results = run_epsilon_sweep(args.alpha, args.N, args.filter, args.max_bits)
    theory_opt, empirical_opt = find_optimal_epsilon(results)

    print(f"\nResults for alpha={args.alpha}:")
    print(
        f"  epsilon*_theory:    bits={theory_opt['fingerprint_bits']}, "
        f"epsilon={theory_opt['epsilon']:.4f}, "
        f"lower_bound={theory_opt['lower_bound']:.3f}"
    )
    print(
        f"  epsilon*_empirical: bits={empirical_opt['fingerprint_bits']}, "
        f"epsilon={empirical_opt['epsilon']:.4f}, "
        f"savings={empirical_opt['empirical_savings']:.3f}"
    )

    plot_epsilon_sweep(results, args.alpha, args.filter, args.N, args.output)

    if args.show:
        plt.show()


if __name__ == "__main__":
    main()
