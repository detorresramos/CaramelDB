"""Parallel dataset generation — splits rows across worker processes.

Each worker writes to a non-overlapping slice of a shared memmap file.

Usage:
    python scripts/generate_empirical_parallel.py \
        --csv PATH --num-rows 100000000 --num-cols 100 \
        --output datasets/asin_100m_m100.npy --workers 12
"""

import argparse
import os
import subprocess
import sys
import time

import numpy as np

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)
from benchmark_multiset import load_empirical_tiers

SEED = 42

WORKER_CODE = '''
import os, sys, time
import numpy as np
sys.path.insert(0, "{script_dir}")
from benchmark_multiset import load_empirical_tiers

csv_path = "{csv_path}"
output_path = "{output_path}"
start_row = {start_row}
end_row = {end_row}
num_rows = {num_rows}
num_cols = {num_cols}
seed = {seed}
worker_id = {worker_id}

tier_starts, tier_counts, tier_probs, _ = load_empirical_tiers(csv_path)
num_tiers = len(tier_counts)
M = num_cols
n = end_row - start_row

# Open existing memmap for writing our slice
values = np.lib.format.open_memmap(output_path, mode="r+")

rng = np.random.default_rng(seed + worker_id)
chunk = 1_000_000
t0 = time.perf_counter()

for cs in range(0, n, chunk):
    ce = min(cs + chunk, n)
    cn = ce - cs

    tiers = rng.choice(num_tiers, size=(cn, M), p=tier_probs)
    offsets = (rng.random((cn, M)) * tier_counts[tiers]).astype(np.int64)
    ids = (tier_starts[tiers] + offsets).astype(np.uint32)

    sorted_ids = np.sort(ids, axis=1)
    has_dup = np.any(sorted_ids[:, 1:] == sorted_ids[:, :-1], axis=1)
    dup_rows = np.where(has_dup)[0]

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

    values[start_row + cs:start_row + ce] = ids

elapsed = time.perf_counter() - t0
print(f"  worker {{worker_id}}: rows [{{start_row:,}}..{{end_row:,}}) done in {{elapsed:.1f}}s", flush=True)
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--csv", required=True)
    parser.add_argument("--num-rows", type=int, required=True)
    parser.add_argument("--num-cols", type=int, required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--workers", type=int, default=12)
    parser.add_argument("--seed", type=int, default=SEED)
    args = parser.parse_args()

    tier_starts, tier_counts, tier_probs, total_unique = load_empirical_tiers(args.csv)
    print(f"Distribution: {total_unique:,} unique items, {len(tier_counts)} tiers")
    print(f"Generating: N={args.num_rows:,}, M={args.num_cols}, workers={args.workers}")

    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)

    # Create the memmap file
    values = np.lib.format.open_memmap(
        args.output, mode="w+", dtype=np.uint32,
        shape=(args.num_rows, args.num_cols),
    )
    del values  # close so workers can open it

    # Split rows across workers
    rows_per_worker = args.num_rows // args.workers
    procs = []
    t0 = time.perf_counter()

    for i in range(args.workers):
        start = i * rows_per_worker
        end = start + rows_per_worker if i < args.workers - 1 else args.num_rows
        code = WORKER_CODE.format(
            script_dir=SCRIPT_DIR,
            csv_path=args.csv,
            output_path=os.path.abspath(args.output),
            start_row=start,
            end_row=end,
            num_rows=args.num_rows,
            num_cols=args.num_cols,
            seed=args.seed,
            worker_id=i,
        )
        python = sys.executable
        proc = subprocess.Popen([python, "-c", code])
        procs.append(proc)

    print(f"Launched {len(procs)} workers", flush=True)

    for proc in procs:
        proc.wait()
        if proc.returncode != 0:
            print(f"Worker failed with exit code {proc.returncode}", flush=True)

    elapsed = time.perf_counter() - t0
    size_mb = os.path.getsize(args.output) / 1e6
    print(f"Done in {elapsed:.1f}s — {args.output} ({size_mb:.0f} MB)")


if __name__ == "__main__":
    main()
