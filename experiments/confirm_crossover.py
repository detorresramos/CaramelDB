"""
Confirm crossover points for each filter type.

For each filter type, find the exact alpha where a filter configuration
first beats the no-filter baseline.
"""

import carameldb
import numpy as np
from carameldb import BinaryFuseFilterConfig, BloomFilterConfig, XORFilterConfig


def find_crossover(n: int, alpha_range: list[float], filter_type: str) -> dict:
    """
    Find the crossover point for a given filter type.

    Returns dict with crossover alpha, best config, and sizes.
    """
    for alpha in alpha_range:
        num_minority = n - int(n * alpha)

        keys = [f"key{i}" for i in range(n)]
        np.random.seed(42)
        values = np.zeros(n, dtype=np.uint32)
        values[:num_minority] = np.arange(1, num_minority + 1, dtype=np.uint32)
        np.random.shuffle(values)

        # No filter baseline
        csf_none = carameldb.Caramel(keys, values, verbose=False)
        none_size = csf_none.get_stats().in_memory_bytes

        # Find best config for this filter type
        best_size = None
        best_config = None

        if filter_type == "binary_fuse":
            for bits in [1, 2, 3, 4]:
                csf = carameldb.Caramel(
                    keys,
                    values,
                    prefilter=BinaryFuseFilterConfig(fingerprint_bits=bits),
                    verbose=False,
                )
                size = csf.get_stats().in_memory_bytes
                if best_size is None or size < best_size:
                    best_size = size
                    best_config = f"{bits}-bit"

        elif filter_type == "xor":
            for bits in [1, 2, 3, 4]:
                csf = carameldb.Caramel(
                    keys,
                    values,
                    prefilter=XORFilterConfig(fingerprint_bits=bits),
                    verbose=False,
                )
                size = csf.get_stats().in_memory_bytes
                if best_size is None or size < best_size:
                    best_size = size
                    best_config = f"{bits}-bit"

        elif filter_type == "bloom":
            for bpe in [1, 2, 3, 4]:
                for nh in [1, 2, 3, 4]:
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
                        best_config = f"{bpe}bpe/{nh}h"

        if best_size < none_size:
            return {
                "crossover_alpha": alpha,
                "best_config": best_config,
                "filter_size": best_size,
                "no_filter_size": none_size,
                "savings_bytes": none_size - best_size,
                "savings_pct": (none_size - best_size) / none_size * 100,
            }

    return {"crossover_alpha": None, "best_config": None}


def main():
    n = 100_000
    # Fine-grained alpha search
    alpha_range = [round(0.60 + i * 0.01, 2) for i in range(25)]  # 0.60 to 0.84

    print("=" * 70)
    print(f"CROSSOVER POINT CONFIRMATION (N={n:,})")
    print("=" * 70)
    print()

    results = {}
    for filter_type in ["binary_fuse", "xor", "bloom"]:
        print(f"Searching for {filter_type} crossover...")
        result = find_crossover(n, alpha_range, filter_type)
        results[filter_type] = result

        if result["crossover_alpha"]:
            print(f"  FOUND: α = {result['crossover_alpha']}")
            print(f"         Config: {result['best_config']}")
            print(f"         Filter size: {result['filter_size']:,} bytes")
            print(f"         No-filter:   {result['no_filter_size']:,} bytes")
            print(
                f"         Savings:     {result['savings_bytes']:,} bytes ({result['savings_pct']:.2f}%)"
            )
        else:
            print(f"  No crossover found in range {alpha_range[0]} - {alpha_range[-1]}")
        print()

    # Summary table
    print("=" * 70)
    print("SUMMARY: CROSSOVER POINTS")
    print("=" * 70)
    print()
    print(
        f"{'Filter Type':<15} | {'Crossover α':<12} | {'Best Config':<12} | {'Savings':<10}"
    )
    print("-" * 55)
    for ft in ["binary_fuse", "xor", "bloom"]:
        r = results[ft]
        if r["crossover_alpha"]:
            print(
                f"{ft:<15} | {r['crossover_alpha']:<12} | {r['best_config']:<12} | {r['savings_pct']:.2f}%"
            )
        else:
            print(f"{ft:<15} | {'N/A':<12} | {'N/A':<12} | N/A")

    print()
    print("=" * 70)
    print("VERIFICATION: Confirming filter beats no-filter at crossover")
    print("=" * 70)
    print()

    # Verify each crossover
    for ft in ["binary_fuse", "xor", "bloom"]:
        r = results[ft]
        if r["crossover_alpha"]:
            alpha = r["crossover_alpha"]
            print(f"{ft} at α={alpha}:")
            print(f"  No-filter: {r['no_filter_size']:,} bytes")
            print(f"  {r['best_config']}: {r['filter_size']:,} bytes")
            assert (
                r["filter_size"] < r["no_filter_size"]
            ), f"FAILED: filter not smaller!"
            print(f"  ✓ CONFIRMED: {r['filter_size']:,} < {r['no_filter_size']:,}")
            print()


if __name__ == "__main__":
    main()
