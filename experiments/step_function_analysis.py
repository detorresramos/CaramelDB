"""
Analyze step functions for optimal filter parameters vs alpha.

This script generates plots showing how the optimal discrete parameters
(fingerprint bits for BinaryFuse/XOR, num_hashes and bits_per_element for Bloom)
change as alpha increases. The discrete nature creates step functions.
"""

import argparse

import carameldb
import matplotlib.pyplot as plt
import numpy as np
from carameldb import BinaryFuseFilterConfig, BloomFilterConfig, XORFilterConfig


def find_optimal_binaryfuse(keys, values, bits_range=[1, 2, 3, 4]):
    """Find optimal fingerprint bits for BinaryFuse."""
    best_bits = None
    best_size = None
    for bits in bits_range:
        csf = carameldb.Caramel(
            keys,
            values,
            prefilter=BinaryFuseFilterConfig(fingerprint_bits=bits),
            verbose=False,
        )
        size = csf.get_stats().in_memory_bytes
        if best_size is None or size < best_size:
            best_size = size
            best_bits = bits
    return best_bits, best_size


def find_optimal_xor(keys, values, bits_range=[1, 2, 3, 4]):
    """Find optimal fingerprint bits for XOR."""
    best_bits = None
    best_size = None
    for bits in bits_range:
        csf = carameldb.Caramel(
            keys,
            values,
            prefilter=XORFilterConfig(fingerprint_bits=bits),
            verbose=False,
        )
        size = csf.get_stats().in_memory_bytes
        if best_size is None or size < best_size:
            best_size = size
            best_bits = bits
    return best_bits, best_size


def find_optimal_bloom(
    keys, values, num_minority, bpe_range=[1, 2, 3, 4], nh_range=[1, 2, 3, 4]
):
    """Find optimal bits_per_element and num_hashes for Bloom."""
    best_bpe = None
    best_nh = None
    best_size = None
    for bpe in bpe_range:
        for nh in nh_range:
            size_bits = max(bpe * num_minority, 64)
            csf = carameldb.Caramel(
                keys,
                values,
                prefilter=BloomFilterConfig(size=size_bits, num_hashes=nh),
                verbose=False,
            )
            size = csf.get_stats().in_memory_bytes
            if best_size is None or size < best_size:
                best_size = size
                best_bpe = bpe
                best_nh = nh
    return best_bpe, best_nh, best_size


def generate_data(keys, values, num_minority):
    """Generate data for a single alpha value."""
    np.random.shuffle(values)
    return keys, values


def run_analysis(n: int, alphas: np.ndarray, filter_type: str, verbose: bool = True):
    """
    Run step function analysis for a given filter type.

    Args:
        n: Number of keys
        alphas: Array of alpha values to test
        filter_type: 'binaryfuse', 'xor', or 'bloom'
        verbose: Print progress

    Returns:
        Dictionary with results
    """
    results = {
        "alphas": alphas.tolist(),
        "filter_type": filter_type,
        "n": n,
    }

    if filter_type == "bloom":
        optimal_bpe = []
        optimal_nh = []
        optimal_sizes = []
    else:
        optimal_bits = []
        optimal_sizes = []

    for i, alpha in enumerate(alphas):
        if verbose and i % 10 == 0:
            print(f"Progress: {i}/{len(alphas)} (α={alpha:.2f})")

        num_minority = n - int(n * alpha)

        keys = [f"key{j}" for j in range(n)]
        np.random.seed(42)
        values = np.zeros(n, dtype=np.uint32)
        values[:num_minority] = np.arange(1, num_minority + 1, dtype=np.uint32)
        np.random.shuffle(values)

        if filter_type == "binaryfuse":
            bits, size = find_optimal_binaryfuse(keys, values)
            optimal_bits.append(bits)
            optimal_sizes.append(size)
        elif filter_type == "xor":
            bits, size = find_optimal_xor(keys, values)
            optimal_bits.append(bits)
            optimal_sizes.append(size)
        elif filter_type == "bloom":
            bpe, nh, size = find_optimal_bloom(keys, values, num_minority)
            optimal_bpe.append(bpe)
            optimal_nh.append(nh)
            optimal_sizes.append(size)

    if filter_type == "bloom":
        results["optimal_bpe"] = optimal_bpe
        results["optimal_nh"] = optimal_nh
    else:
        results["optimal_bits"] = optimal_bits
    results["optimal_sizes"] = optimal_sizes

    if verbose:
        print("Done")

    return results


def find_transitions(alphas, values):
    """Find transition points where value changes."""
    transitions = []
    prev = values[0]
    for a, v in zip(alphas, values):
        if v != prev:
            transitions.append((a, prev, v))
            prev = v
    return transitions


def plot_step_function(results, output_path: str, show: bool = False):
    """Plot step function for filter parameters."""
    alphas = results["alphas"]
    filter_type = results["filter_type"]
    n = results["n"]

    if filter_type == "bloom":
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))

        # Plot bits_per_element
        ax1.step(
            alphas, results["optimal_bpe"], where="mid", linewidth=2.5, color="#1f77b4"
        )
        ax1.scatter(alphas, results["optimal_bpe"], s=20, color="#1f77b4", zorder=5)
        ax1.set_xlabel("α (frequency of most common element)", fontsize=12)
        ax1.set_ylabel("Optimal Bits per Element", fontsize=12)
        ax1.set_title("Bloom: Optimal bits_per_element vs α", fontsize=12)
        ax1.set_yticks([1, 2, 3, 4])
        ax1.set_ylim(0.5, 4.5)
        ax1.grid(True, alpha=0.3)

        # Mark transitions
        transitions = find_transitions(alphas, results["optimal_bpe"])
        for a, prev, curr in transitions:
            ax1.axvline(x=a, color="gray", linestyle="--", alpha=0.5)
            ax1.annotate(f"α={a:.2f}", xy=(a, curr + 0.2), fontsize=9, ha="center")

        # Plot num_hashes
        ax2.step(
            alphas, results["optimal_nh"], where="mid", linewidth=2.5, color="#2ca02c"
        )
        ax2.scatter(alphas, results["optimal_nh"], s=20, color="#2ca02c", zorder=5)
        ax2.set_xlabel("α (frequency of most common element)", fontsize=12)
        ax2.set_ylabel("Optimal Number of Hashes", fontsize=12)
        ax2.set_title("Bloom: Optimal num_hashes vs α", fontsize=12)
        ax2.set_yticks([1, 2, 3, 4])
        ax2.set_ylim(0.5, 4.5)
        ax2.grid(True, alpha=0.3)

        # Mark transitions
        transitions = find_transitions(alphas, results["optimal_nh"])
        for a, prev, curr in transitions:
            ax2.axvline(x=a, color="gray", linestyle="--", alpha=0.5)
            ax2.annotate(f"α={a:.2f}", xy=(a, curr + 0.2), fontsize=9, ha="center")

        plt.suptitle(f"Bloom Filter: Optimal Parameters vs α (N={n:,})", fontsize=14)
    else:
        fig, ax = plt.subplots(figsize=(10, 6))

        color = "#d62728" if filter_type == "binaryfuse" else "#ff7f0e"
        ax.step(
            alphas, results["optimal_bits"], where="mid", linewidth=2.5, color=color
        )
        ax.scatter(alphas, results["optimal_bits"], s=20, color=color, zorder=5)
        ax.set_xlabel("α (frequency of most common element)", fontsize=12)
        ax.set_ylabel("Optimal Fingerprint Bits", fontsize=12)
        ax.set_title(
            f"{filter_type.title()}: Optimal Fingerprint Bits vs α (N={n:,})",
            fontsize=14,
        )
        ax.set_yticks([1, 2, 3, 4])
        ax.set_ylim(0.5, 4.5)
        ax.grid(True, alpha=0.3)

        # Mark transitions
        transitions = find_transitions(alphas, results["optimal_bits"])
        for a, prev, curr in transitions:
            ax.axvline(x=a, color="gray", linestyle="--", alpha=0.5)
            ax.annotate(f"α={a:.2f}", xy=(a, curr + 0.2), fontsize=9, ha="center")

    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches="tight")
    print(f"Saved: {output_path}")

    if show:
        plt.show()
    else:
        plt.close()


def print_transitions(results):
    """Print transition points."""
    alphas = results["alphas"]
    filter_type = results["filter_type"]

    if filter_type == "bloom":
        print(f"\n=== BLOOM BITS_PER_ELEMENT TRANSITIONS ===")
        transitions = find_transitions(alphas, results["optimal_bpe"])
        print(f'Start: {results["optimal_bpe"][0]} bpe')
        for a, prev, curr in transitions:
            print(f"α = {a:.2f}: {prev} → {curr} bpe")

        print(f"\n=== BLOOM NUM_HASHES TRANSITIONS ===")
        transitions = find_transitions(alphas, results["optimal_nh"])
        print(f'Start: {results["optimal_nh"][0]} hashes')
        for a, prev, curr in transitions:
            print(f"α = {a:.2f}: {prev} → {curr} hashes")
    else:
        print(f"\n=== {filter_type.upper()} TRANSITIONS ===")
        transitions = find_transitions(alphas, results["optimal_bits"])
        print(f'Start: {results["optimal_bits"][0]}-bit')
        for a, prev, curr in transitions:
            print(f"α = {a:.2f}: {prev}-bit → {curr}-bit")


def main():
    parser = argparse.ArgumentParser(
        description="Analyze step functions for optimal filter parameters"
    )
    parser.add_argument(
        "--filter",
        "-f",
        type=str,
        default="binaryfuse",
        choices=["binaryfuse", "xor", "bloom"],
        help="Filter type to analyze",
    )
    parser.add_argument("--n", type=int, default=100000, help="Number of keys")
    parser.add_argument(
        "--alpha-start", type=float, default=0.65, help="Starting alpha value"
    )
    parser.add_argument(
        "--alpha-end", type=float, default=0.96, help="Ending alpha value"
    )
    parser.add_argument(
        "--alpha-step", type=float, default=0.01, help="Alpha step size"
    )
    parser.add_argument(
        "--output", "-o", type=str, default=None, help="Output path for plot"
    )
    parser.add_argument("--show", action="store_true", help="Show plot")

    args = parser.parse_args()

    alphas = np.arange(args.alpha_start, args.alpha_end, args.alpha_step)

    print(f"Analyzing {args.filter} filter")
    print(f"N = {args.n:,}")
    print(
        f"Alpha range: {args.alpha_start} to {args.alpha_end} (step {args.alpha_step})"
    )
    print(f"Total configurations: {len(alphas)}")
    print()

    results = run_analysis(args.n, alphas, args.filter)

    print_transitions(results)

    output_path = args.output or f"{args.filter}_step_function.png"
    plot_step_function(results, output_path, show=args.show)


if __name__ == "__main__":
    main()
