"""
Find the optimal alpha (normalized frequency of most common element) for different filters.

This script performs a binary search to find the alpha threshold where using a filter
(XOR or Binary Fuse) results in smaller CSF file size compared to no filter.
"""

import os
import sys
import tempfile

import carameldb
import numpy as np
from carameldb import BinaryFuseFilterConfig, XORFilterConfig


def gen_str_keys(n):
    """Generate n string keys."""
    return [f"key{i}" for i in range(n)]


def measure_sizes(n, alpha, filter_config):
    """
    Measure CSF sizes with and without filter.

    Args:
        n: Number of elements
        alpha: Normalized frequency of most common element
        filter_config: Filter configuration

    Returns:
        (size_with_filter, size_without_filter)
    """
    keys = gen_str_keys(n)

    # Create data distribution based on alpha
    num_most_common = int(n * alpha)
    num_other = n - num_most_common
    values = [10000 for _ in range(num_most_common)] + [i for i in range(num_other)]

    # Measure with filter
    csf_with = carameldb.Caramel(keys, values, prefilter=filter_config)
    with tempfile.NamedTemporaryFile(suffix=".csf", delete=False) as tmp:
        tmp_path_with = tmp.name
    try:
        csf_with.save(tmp_path_with)
        size_with = os.path.getsize(tmp_path_with)
    finally:
        if os.path.exists(tmp_path_with):
            os.remove(tmp_path_with)

    # Measure without filter
    csf_without = carameldb.Caramel(keys, values, prefilter=None)
    with tempfile.NamedTemporaryFile(suffix=".csf", delete=False) as tmp:
        tmp_path_without = tmp.name
    try:
        csf_without.save(tmp_path_without)
        size_without = os.path.getsize(tmp_path_without)
    finally:
        if os.path.exists(tmp_path_without):
            os.remove(tmp_path_without)

    return size_with, size_without


def find_optimal_alpha(filter_type, n, precision=0.01, verbose=True):
    """
    Find the optimal alpha using binary search.

    The optimal alpha is the threshold where filter size becomes smaller than no-filter size.

    Args:
        filter_type: 'xor' or 'binary_fuse'
        n: Number of elements
        precision: Precision for alpha (e.g., 0.01 means find to nearest 0.01)
        verbose: Print progress

    Returns:
        Optimal alpha value
    """
    if filter_type == "xor":
        filter_config = XORFilterConfig()
    elif filter_type == "binary_fuse":
        filter_config = BinaryFuseFilterConfig()
    else:
        raise ValueError(f"Unknown filter type: {filter_type}")

    # Binary search for the crossover point
    # Start with a narrower range - filters are typically only useful at high alpha
    left, right = 0.6, 1.0

    # First, do a coarse sweep to find the approximate range
    alphas_to_test = np.arange(0.6, 1.01, 0.05)
    results = []

    if verbose:
        print(f"\n{'='*70}")
        print(f"Finding optimal alpha for {filter_type} filter with N={n}")
        print(f"{'='*70}")
        print(
            f"{'Alpha':>8} {'With Filter':>15} {'Without Filter':>15} {'Difference':>12}"
        )
        print(f"{'-'*70}")

    for alpha in alphas_to_test:
        size_with, size_without = measure_sizes(n, alpha, filter_config)
        diff = size_with - size_without
        results.append((alpha, size_with, size_without, diff))
        if verbose:
            print(f"{alpha:>8.2f} {size_with:>15,} {size_without:>15,} {diff:>12,}")

    # Find where the sign changes (filter becomes beneficial)
    crossover_idx = None
    for i in range(len(results) - 1):
        if results[i][3] > 0 and results[i + 1][3] < 0:
            crossover_idx = i
            break

    if crossover_idx is None:
        # Check if filter is always better or always worse
        if results[-1][3] > 0:
            if verbose:
                print(f"\nFilter is never beneficial (even at alpha=1.0)")
            return None
        else:
            # Filter is beneficial even at low alpha, narrow search
            left = 0.6
            right = results[0][0]
    else:
        left = results[crossover_idx][0]
        right = results[crossover_idx + 1][0]

    if verbose:
        print(f"\nRefining search between alpha={left:.2f} and alpha={right:.2f}")
        print(f"{'-'*70}")

    # Binary search within the range
    while (right - left) > precision:
        mid = (left + right) / 2
        size_with, size_without = measure_sizes(n, mid, filter_config)
        diff = size_with - size_without

        if verbose:
            print(f"{mid:>8.3f} {size_with:>15,} {size_without:>15,} {diff:>12,}")

        if diff > 0:
            # Filter is worse, need higher alpha
            left = mid
        else:
            # Filter is better, can try lower alpha
            right = mid

    optimal = (left + right) / 2

    if verbose:
        print(f"{'='*70}")
        print(f"Optimal alpha for {filter_type} with N={n}: {optimal:.2f}")
        print(f"{'='*70}\n")

    return optimal


def main():
    if len(sys.argv) < 2:
        print("Usage: python find_optimal_alpha.py <filter_type> [n_values...]")
        print("  filter_type: 'xor' or 'binary_fuse'")
        print(
            "  n_values: Space-separated list of N values to test (default: 1000 10000 100000 1000000 10000000)"
        )
        sys.exit(1)

    filter_type = sys.argv[1].lower()

    if len(sys.argv) > 2:
        n_values = [int(x) for x in sys.argv[2:]]
    else:
        n_values = [1000, 10000, 100000, 1000000, 10000000]

    results = {}
    for n in n_values:
        optimal = find_optimal_alpha(filter_type, n)
        results[n] = optimal

    print("\n" + "=" * 70)
    print(f"SUMMARY: Optimal alpha values for {filter_type.upper()} filter")
    print("=" * 70)
    for n, alpha in results.items():
        n_str = f"{n:,}" if n >= 1000 else str(n)
        if alpha is None:
            print(f"N = {n_str:>12}: Filter never beneficial")
        else:
            print(f"N = {n_str:>12}: α ≥ {alpha:.2f}")
    print("=" * 70)


if __name__ == "__main__":
    main()
