"""Generate and save empirical multiset datasets as .npy files.

Usage:
    python scripts/generate_empirical_dataset.py --csv PATH --num-rows N --num-cols M --output PATH

Example:
    python scripts/generate_empirical_dataset.py \
        --csv /path/to/caramel_asin_click_distribution.csv \
        --num-rows 100000000 --num-cols 10 \
        --output datasets/asin_100m_m10.npy
"""

import argparse
import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from benchmark_multiset import load_empirical_tiers, sample_empirical

SEED = 42


def main():
    parser = argparse.ArgumentParser(description="Generate empirical multiset dataset")
    parser.add_argument("--csv", required=True, help="Path to (metric,count) CSV")
    parser.add_argument("--num-rows", type=int, required=True, help="Number of keys (N)")
    parser.add_argument("--num-cols", type=int, required=True, help="Values per key (M)")
    parser.add_argument("--output", required=True, help="Output .npy path")
    parser.add_argument("--seed", type=int, default=SEED)
    args = parser.parse_args()

    tier_starts, tier_counts, tier_probs, total_unique = load_empirical_tiers(args.csv)
    print(f"Distribution: {total_unique:,} unique items, {len(tier_counts)} tiers")
    print(f"Generating: N={args.num_rows:,}, M={args.num_cols}, seed={args.seed}")

    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)

    rng = np.random.default_rng(args.seed)
    t0 = time.perf_counter()

    # Use memory-mapped file so only one chunk lives in RAM at a time.
    values = np.lib.format.open_memmap(
        args.output, mode="w+", dtype=np.uint32,
        shape=(args.num_rows, args.num_cols),
    )

    chunk_size = 5_000_000
    num_tiers = len(tier_counts)
    M = args.num_cols

    for start in range(0, args.num_rows, chunk_size):
        end = min(start + chunk_size, args.num_rows)
        n = end - start

        # Sample exactly M per row (no oversample — V >> M so collisions rare)
        tiers = rng.choice(num_tiers, size=(n, M), p=tier_probs)
        offsets = (rng.random((n, M)) * tier_counts[tiers]).astype(np.int64)
        ids = (tier_starts[tiers] + offsets).astype(np.uint32)

        # Vectorized dup check: sort each row, compare adjacent
        sorted_ids = np.sort(ids, axis=1)
        has_dup = np.any(sorted_ids[:, 1:] == sorted_ids[:, :-1], axis=1)
        dup_rows = np.where(has_dup)[0]

        # Fix the rare dup rows by rejection sampling
        for i in dup_rows:
            seen = set()
            row = []
            while len(row) < M:
                t = rng.choice(num_tiers, p=tier_probs)
                gid = int(tier_starts[t] + rng.integers(tier_counts[t]))
                if gid not in seen:
                    seen.add(gid)
                    row.append(gid)
            ids[i] = np.array(row, dtype=np.uint32)

        values[start:end] = ids

        elapsed = time.perf_counter() - t0
        pct = end / args.num_rows * 100
        print(f"  {end:>12,} / {args.num_rows:,} ({pct:5.1f}%) — {elapsed:.0f}s"
              f"  dups={len(dup_rows)}", flush=True)

    values.flush()
    sample_time = time.perf_counter() - t0
    size_mb = os.path.getsize(args.output) / 1e6
    print(f"Done in {sample_time:.1f}s — {args.output} ({size_mb:.0f} MB)")


if __name__ == "__main__":
    main()
