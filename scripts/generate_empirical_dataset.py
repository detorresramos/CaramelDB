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

    chunk_size = 1_000_000
    num_tiers = len(tier_counts)
    oversample = 2.0
    draws_per_row = max(int(args.num_cols * oversample), args.num_cols + 8)

    for start in range(0, args.num_rows, chunk_size):
        end = min(start + chunk_size, args.num_rows)
        n = end - start
        total_draws = n * draws_per_row

        tiers = rng.choice(num_tiers, size=total_draws, p=tier_probs)
        offsets = (rng.random(total_draws) * tier_counts[tiers]).astype(np.int64)
        ids_flat = (tier_starts[tiers] + offsets).astype(np.uint64)
        ids = ids_flat.reshape(n, draws_per_row)

        for i in range(n):
            _, first_idx = np.unique(ids[i], return_index=True)
            uniq = ids[i][np.sort(first_idx)]
            if len(uniq) >= args.num_cols:
                values[start + i] = uniq[:args.num_cols].astype(np.uint32)
            else:
                seen = set(int(x) for x in uniq)
                while len(seen) < args.num_cols:
                    t = rng.choice(num_tiers, p=tier_probs)
                    gid = int(tier_starts[t] + rng.integers(tier_counts[t]))
                    seen.add(gid)
                values[start + i] = np.fromiter(seen, dtype=np.uint32, count=args.num_cols)

        elapsed = time.perf_counter() - t0
        pct = end / args.num_rows * 100
        print(f"  {end:>12,} / {args.num_rows:,} ({pct:5.1f}%) — {elapsed:.0f}s", flush=True)

    values.flush()
    sample_time = time.perf_counter() - t0
    size_mb = os.path.getsize(args.output) / 1e6
    print(f"Done in {sample_time:.1f}s — {args.output} ({size_mb:.0f} MB)")


if __name__ == "__main__":
    main()
