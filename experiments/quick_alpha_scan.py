"""
Quick scan to find optimal alpha for filters.

Tests a range of alpha values and finds where filter becomes beneficial.
"""

import os
import sys
import tempfile
import carameldb
from carameldb import XORFilterConfig, BinaryFuseFilterConfig, BloomFilterConfig


def gen_str_keys(n):
    """Generate n string keys."""
    return [f"key{i}" for i in range(n)]


def measure_sizes(n, alpha, filter_config):
    """
    Measure CSF sizes with and without filter.

    Returns:
        (size_with_filter, size_without_filter, difference)
    """
    keys = gen_str_keys(n)

    # Create data distribution based on alpha
    num_most_common = int(n * alpha)
    num_other = n - num_most_common
    values = [10000 for _ in range(num_most_common)] + [i for i in range(num_other)]

    # Measure with filter
    csf_with = carameldb.Caramel(keys, values, prefilter=filter_config, verbose=False)
    with tempfile.NamedTemporaryFile(suffix='.csf', delete=False) as tmp:
        tmp_path_with = tmp.name
    try:
        csf_with.save(tmp_path_with)
        size_with = os.path.getsize(tmp_path_with)
    finally:
        if os.path.exists(tmp_path_with):
            os.remove(tmp_path_with)

    # Measure without filter
    csf_without = carameldb.Caramel(keys, values, prefilter=None, verbose=False)
    with tempfile.NamedTemporaryFile(suffix='.csf', delete=False) as tmp:
        tmp_path_without = tmp.name
    try:
        csf_without.save(tmp_path_without)
        size_without = os.path.getsize(tmp_path_without)
    finally:
        if os.path.exists(tmp_path_without):
            os.remove(tmp_path_without)

    return size_with, size_without, size_with - size_without


def find_optimal_alpha(filter_type, n):
    """
    Find optimal alpha by scanning from high to low.
    """
    if filter_type == 'xor':
        filter_config = XORFilterConfig()
    elif filter_type == 'binary_fuse':
        filter_config = BinaryFuseFilterConfig()
    elif filter_type == 'bloom':
        filter_config = BloomFilterConfig()
    else:
        raise ValueError(f"Unknown filter type: {filter_type}")

    print(f"\nFinding optimal alpha for {filter_type.upper()} with N={n:,}")
    print(f"{'Alpha':>8} {'With':>12} {'Without':>12} {'Diff':>10} {'Winner':>10}")
    print("-" * 60)

    # Scan from high alpha down to find crossover
    alphas = [0.95, 0.90, 0.85, 0.80, 0.75, 0.70, 0.65, 0.60]
    results = []

    for alpha in alphas:
        size_with, size_without, diff = measure_sizes(n, alpha, filter_config)
        winner = "filter" if diff < 0 else "no-filter"
        results.append((alpha, size_with, size_without, diff))
        print(f"{alpha:>8.2f} {size_with:>12,} {size_without:>12,} {diff:>10,} {winner:>10}")

    # Find crossover point (where filter stops being beneficial as alpha decreases)
    optimal = None
    for i in range(len(results) - 1):
        if results[i][3] < 0 and results[i+1][3] > 0:
            # Filter is beneficial at alphas[i] but not at alphas[i+1]
            # Crossover is between these two values
            mid_alpha = (alphas[i] + alphas[i+1]) / 2
            size_with, size_without, diff = measure_sizes(n, mid_alpha, filter_config)
            winner = "filter" if diff < 0 else "no-filter"
            print(f"{mid_alpha:>8.2f} {size_with:>12,} {size_without:>12,} {diff:>10,} {winner:>10}")

            if diff < 0:
                # Filter still beneficial at mid_alpha, crossover is between mid and alphas[i+1]
                optimal = (mid_alpha + alphas[i+1]) / 2
            else:
                # Filter not beneficial at mid_alpha, crossover is between alphas[i] and mid
                optimal = (alphas[i] + mid_alpha) / 2
            break

    if optimal is None:
        if results[0][3] < 0:
            # Filter is beneficial at highest alpha tested
            optimal = alphas[0]
            print(f"\nFilter beneficial at alpha={alphas[0]:.2f}")
        else:
            optimal = None
            print(f"\nFilter never beneficial")

    if optimal:
        print(f"\nOptimal alpha ≈ {optimal:.2f}")

    return optimal


def main():
    if len(sys.argv) < 2:
        print("Usage: python quick_alpha_scan.py <filter_type> [n_values...]")
        print("  filter_type: 'xor', 'binary_fuse', or 'bloom'")
        print("  n_values: Space-separated list of N values")
        sys.exit(1)

    filter_type = sys.argv[1].lower()

    if len(sys.argv) > 2:
        n_values = [int(x) for x in sys.argv[2:]]
    else:
        n_values = [1000, 10000, 100000, 1000000, 10000000]

    print("=" * 70)
    print(f"Quick Alpha Scan for {filter_type.upper()} Filter")
    print("=" * 70)

    results = {}
    for n in n_values:
        optimal = find_optimal_alpha(filter_type, n)
        results[n] = optimal

    print("\n" + "=" * 70)
    print(f"SUMMARY: Optimal alpha values for {filter_type.upper()} filter")
    print("=" * 70)
    for n, alpha in results.items():
        if alpha is None:
            print(f"N = {n:>12,}: Never beneficial")
        else:
            print(f"N = {n:>12,}: α ≥ {alpha:.2f}")
    print("=" * 70)


if __name__ == "__main__":
    main()
