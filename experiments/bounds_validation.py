"""
Experimental Validation of Prefilter Bounds.

Plots theoretical lower and upper bound curves from PREFILTER_THEORY.md,
then overlays empirical measurements to validate where the filtered CSF
actually lies between the bounds.
"""

import argparse
from typing import Dict, List

import matplotlib.pyplot as plt
import numpy as np
from data_gen import gen_alpha_values, gen_keys
from measure import measure_csf

DELTA = 1.089  # Constant for 3 hash functions


def binary_entropy(alpha: float) -> float:
    """
    Compute binary entropy H(alpha) = -alpha*log2(alpha) - (1-alpha)*log2(1-alpha).
    """
    if alpha <= 0 or alpha >= 1:
        return 0.0
    return -alpha * np.log2(alpha) - (1 - alpha) * np.log2(1 - alpha)


def get_filter_params(
    filter_type: str,
    fingerprint_bits: int = None,
    bloom_bits: float = None,
    bloom_hashes: int = None,
) -> tuple[float, float]:
    """
    Return (b_epsilon, epsilon) for a filter configuration.

    Args:
        filter_type: 'xor', 'binary_fuse', or 'bloom'
        fingerprint_bits: For XOR/Binary Fuse filters
        bloom_bits: Bits per element for Bloom filter
        bloom_hashes: Number of hash functions for Bloom filter

    Returns:
        Tuple of (bits per key b(epsilon), false positive rate epsilon)
    """
    if filter_type == "xor":
        b_eps = 1.23 * fingerprint_bits
        epsilon = 2 ** (-fingerprint_bits)
    elif filter_type == "binary_fuse":
        b_eps = 1.125 * fingerprint_bits
        epsilon = 2 ** (-fingerprint_bits)
    elif filter_type == "bloom":
        b_eps = bloom_bits
        k = bloom_hashes
        epsilon = (1 - np.exp(-k / bloom_bits)) ** k
    else:
        raise ValueError(f"Unknown filter type: {filter_type}")
    return b_eps, epsilon


def theoretical_lower_bound(
    alpha: float, epsilon: float, b_eps: float, n_over_N: float, l_0: float = 1.0
) -> float:
    """
    Lower bound on E[savings]/N (tight).

    From PREFILTER_THEORY.md lines 234-237:
        E[savings]/N >= delta * l_0 * alpha * (1 - epsilon) - n/N - b(epsilon) * (1 - alpha)

    Args:
        alpha: Frequency of majority value (f_0/N)
        epsilon: False positive rate of filter
        b_eps: Bits per key for filter b(epsilon)
        n_over_N: Number of unique values divided by total keys (n/N)
        l_0: Huffman code length of majority value (1 when alpha > 0.5)

    Returns:
        Lower bound on bits per key saved
    """
    return DELTA * l_0 * alpha * (1 - epsilon) - n_over_N - b_eps * (1 - alpha)


def theoretical_upper_bound_tight(
    alpha: float, epsilon: float, b_eps: float, n_over_N: float
) -> float:
    """
    Upper bound on E[savings]/N (TIGHT version).

    From PREFILTER_THEORY.md lines 316-319:
        E[savings]/N <= delta * (1 + H(alpha) - 2*alpha*epsilon*(1-alpha)/(3-alpha))
                        + n/N - b(epsilon) * (1 - alpha)

    where H(alpha) = -alpha*log2(alpha) - (1-alpha)*log2(1-alpha) is binary entropy.

    Args:
        alpha: Frequency of majority value (f_0/N)
        epsilon: False positive rate of filter
        b_eps: Bits per key for filter b(epsilon)
        n_over_N: Number of unique values divided by total keys (n/N)

    Returns:
        Upper bound on bits per key saved
    """
    H_alpha = binary_entropy(alpha)
    correction_term = 2 * alpha * epsilon * (1 - alpha) / (3 - alpha)
    return DELTA * (1 + H_alpha - correction_term) + n_over_N - b_eps * (1 - alpha)


def theoretical_crossover_lower(
    epsilon: float, b_eps: float, l_0: float = 1.0, include_n_over_N: bool = True
) -> float:
    """
    Find alpha where lower bound = 0 (threshold beta below which filtering
    is guaranteed to be worse).

    With n/N ≈ (1-α) for unique minority distribution:
        lower = δ·l₀·α·(1-ε) - (1-α) - b(ε)·(1-α)
              = δ·l₀·α·(1-ε) - (1-α)·(1 + b(ε))

    Setting to zero:
        α = (1 + b(ε)) / (δ·l₀·(1-ε) + 1 + b(ε))

    Args:
        epsilon: False positive rate
        b_eps: Bits per key for filter
        l_0: Huffman code length of majority (default 1)
        include_n_over_N: If True, include n/N ≈ (1-α) term in calculation

    Returns:
        Crossover alpha value
    """
    if include_n_over_N:
        return (1 + b_eps) / (DELTA * l_0 * (1 - epsilon) + 1 + b_eps)
    else:
        return b_eps / (DELTA * l_0 * (1 - epsilon) + b_eps)


def run_experiment(
    alphas: List[float],
    N: int,
    filter_type: str,
    fingerprint_bits: int = None,
    bloom_bits: float = None,
    bloom_hashes: int = None,
    minority_dist: str = "unique",
    seed: int = 42,
) -> List[Dict]:
    """
    Run bounds validation experiment across a range of alpha values.

    Args:
        alphas: List of alpha values to test
        N: Total number of keys
        filter_type: 'xor', 'binary_fuse', or 'bloom'
        fingerprint_bits: For XOR/Binary Fuse filters
        bloom_bits: Bits per element for Bloom filter
        bloom_hashes: Number of hash functions for Bloom filter
        minority_dist: Distribution of minority values
        seed: Random seed

    Returns:
        List of result dictionaries with alpha, empirical_savings, etc.
    """
    results = []

    for alpha in alphas:
        keys = gen_keys(N)
        values = gen_alpha_values(N, alpha, seed, minority_dist)

        # Baseline (no filter)
        baseline = measure_csf(
            keys, values, "none", N, alpha, minority_dist=minority_dist
        )

        # Filtered
        filtered = measure_csf(
            keys,
            values,
            filter_type,
            N,
            alpha,
            fingerprint_bits=fingerprint_bits,
            bloom_bits_per_element=int(bloom_bits) if bloom_bits else None,
            bloom_num_hashes=bloom_hashes,
            minority_dist=minority_dist,
        )

        # Empirical savings (bits per key): positive means filter helps
        empirical_savings = (baseline.total_bytes - filtered.total_bytes) * 8 / N

        results.append(
            {
                "alpha": alpha,
                "empirical_savings": empirical_savings,
                "baseline_bpk": baseline.bits_per_key,
                "filtered_bpk": filtered.bits_per_key,
                "n": baseline.huffman_num_symbols,
            }
        )

    return results


def plot_bounds_validation(
    results: List[Dict],
    filter_name: str,
    b_eps: float,
    epsilon: float,
    N: int,
    output_path: str = None,
) -> tuple:
    """
    Plot theoretical bounds vs empirical measurements.

    Args:
        results: List of experiment results
        filter_name: Display name for the filter
        b_eps: Bits per key for filter
        epsilon: False positive rate
        N: Total number of keys
        output_path: Optional path to save figure

    Returns:
        Tuple of (figure, axes)
    """
    alphas = [r["alpha"] for r in results]
    empirical = [r["empirical_savings"] for r in results]
    n_over_N_values = [r["n"] / N for r in results]

    # Compute bounds using actual n/N for each alpha
    lower = [
        theoretical_lower_bound(a, epsilon, b_eps, n_N)
        for a, n_N in zip(alphas, n_over_N_values)
    ]
    upper = [
        theoretical_upper_bound_tight(a, epsilon, b_eps, n_N)
        for a, n_N in zip(alphas, n_over_N_values)
    ]

    # Crossover point (where lower bound = 0)
    crossover = theoretical_crossover_lower(epsilon, b_eps)

    # Create plot
    fig, ax = plt.subplots(figsize=(12, 7))

    # Shaded region between bounds
    ax.fill_between(
        alphas,
        lower,
        upper,
        alpha=0.3,
        color="blue",
        label="Theoretical bounds (tight)",
    )

    # Bound lines
    ax.plot(alphas, lower, "b--", linewidth=1.5, label="Lower bound")
    ax.plot(alphas, upper, "b-", linewidth=1.5, label="Upper bound")

    # Empirical measurements
    ax.plot(
        alphas,
        empirical,
        "ro-",
        markersize=4,
        linewidth=2,
        label="Empirical (actual CSF)",
    )

    # Reference lines
    ax.axhline(y=0, color="k", linestyle="-", alpha=0.5, linewidth=1)
    ax.axvline(
        x=crossover,
        color="r",
        linestyle="--",
        alpha=0.7,
        label=f"Theory crossover (beta={crossover:.3f})",
    )

    # Find empirical crossover
    for i in range(len(empirical) - 1):
        if empirical[i] < 0 and empirical[i + 1] >= 0:
            emp_crossover = alphas[i] + (alphas[i + 1] - alphas[i]) * (
                -empirical[i]
            ) / (empirical[i + 1] - empirical[i])
            ax.axvline(
                x=emp_crossover,
                color="green",
                linestyle="--",
                alpha=0.7,
                label=f"Empirical crossover ({emp_crossover:.3f})",
            )
            break

    ax.set_xlabel("alpha (frequency of majority value)", fontsize=12)
    ax.set_ylabel("Bits per key saved (positive = filter helps)", fontsize=12)
    ax.set_title(
        f"{filter_name}: Theoretical Bounds vs Empirical\n"
        f"b(epsilon)={b_eps:.3f}, epsilon={epsilon:.3f}, N={N:,}",
        fontsize=14,
    )
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
        description="Validate theoretical prefilter bounds against empirical measurements"
    )
    parser.add_argument(
        "--filter",
        choices=["xor", "binary_fuse", "bloom"],
        required=True,
        help="Filter type to test",
    )
    parser.add_argument(
        "--fingerprint-bits",
        type=int,
        default=1,
        help="Fingerprint bits for XOR/Binary Fuse (default: 1)",
    )
    parser.add_argument(
        "--bloom-bits",
        type=float,
        default=1,
        help="Bits per element for Bloom filter (default: 1)",
    )
    parser.add_argument(
        "--bloom-hashes",
        type=int,
        default=1,
        help="Number of hash functions for Bloom filter (default: 1)",
    )
    parser.add_argument(
        "--N", type=int, default=100_000, help="Number of keys (default: 100,000)"
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
        "-o", "--output", type=str, default=None, help="Output file path for figure"
    )
    parser.add_argument(
        "--show", action="store_true", help="Display the plot interactively"
    )

    args = parser.parse_args()

    alphas = list(np.arange(args.alpha_start, args.alpha_end + 0.001, args.alpha_step))

    # Get filter params and run experiment
    if args.filter in ("xor", "binary_fuse"):
        b_eps, epsilon = get_filter_params(args.filter, args.fingerprint_bits)
        results = run_experiment(
            alphas, args.N, args.filter, fingerprint_bits=args.fingerprint_bits
        )
        filter_name = (
            f"{args.filter.replace('_', ' ').title()} ({args.fingerprint_bits}-bit)"
        )
    else:
        b_eps, epsilon = get_filter_params(
            "bloom", bloom_bits=args.bloom_bits, bloom_hashes=args.bloom_hashes
        )
        results = run_experiment(
            alphas,
            args.N,
            "bloom",
            bloom_bits=args.bloom_bits,
            bloom_hashes=args.bloom_hashes,
        )
        filter_name = f"Bloom ({int(args.bloom_bits)}b/{args.bloom_hashes}h)"

    fig, ax = plot_bounds_validation(
        results, filter_name, b_eps, epsilon, args.N, args.output
    )

    if args.show:
        plt.show()


if __name__ == "__main__":
    main()
