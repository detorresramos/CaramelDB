"""Generate multiset datasets as memory-mapped .npy files.

Each row is a list of M uint32 item IDs sampled without replacement from the
chosen distribution. Output is a 2D (num_rows, num_cols) uint32 array written
as an .npy memmap so generation is bounded by ~chunk-size RAM regardless of
total output size.

Distributions:
  csv      — empirical tier distribution from a (metric, count) CSV
  uniform  — all V items equally probable
  zipfian  — rank-based weights p_i ~ 1/i^s, truncated to V items

CSV format (for `csv`): one header row, then rows of two columns. Each row
`(metric, count)` means `count` distinct items each carry weight `metric`.
Items are assigned contiguous integer IDs, starting from 0, in CSV row order.
Example:

    metric,count
    1.0,500000       # 500K items each appearing once (total weight 500K)
    2.0,100000       # 100K items each appearing twice (total weight 200K)
    100.0,10         # 10 items with weight 100 (the heavy tail)

Usage:
  # Empirical (from real-world distribution CSV)
  python scripts/generate_dataset.py csv <csv_path> \\
      --num-rows 100000000 --num-cols 100 \\
      --output datasets/asin_100m_m100.npy

  # Uniform over 10M items
  python scripts/generate_dataset.py uniform --vocab 10000000 \\
      --num-rows 1000000 --num-cols 50 --output datasets/uniform.npy

  # Zipfian with exponent 1.2, truncated to 1M items
  python scripts/generate_dataset.py zipfian --vocab 1000000 --exponent 1.2 \\
      --num-rows 1000000 --num-cols 50 --output datasets/zipf.npy
"""

import argparse
import csv as csvlib
import multiprocessing as mp
import os
import time
from dataclasses import dataclass
from typing import Optional

import numpy as np

CHUNK_ROWS = 1_000_000
DEFAULT_SEED = 42


@dataclass(frozen=True)
class DistSpec:
    kind: str                                   # "csv" | "uniform" | "zipfian"
    csv_path: Optional[str] = None
    vocab: Optional[int] = None
    exponent: Optional[float] = None


def load_csv_tiers(csv_path):
    metrics, counts = [], []
    with open(csv_path, newline="") as f:
        reader = csvlib.reader(f)
        next(reader, None)
        for row in reader:
            if row:
                metrics.append(float(row[0]))
                counts.append(int(row[1]))
    metrics = np.asarray(metrics, dtype=np.float64)
    counts = np.asarray(counts, dtype=np.int64)
    weights = metrics * counts
    probs = weights / weights.sum()
    starts = np.concatenate([[0], np.cumsum(counts[:-1])]).astype(np.int64)
    return starts, counts, probs, int(counts.sum())


def _rejection_fix(ids, dup_rows, draw_one_fn, M, rng):
    for i in dup_rows:
        seen = set()
        row = []
        while len(row) < M:
            gid = draw_one_fn(rng)
            if gid not in seen:
                seen.add(gid)
                row.append(gid)
        ids[i] = np.asarray(row, dtype=np.uint32)


def sample_csv_chunk(starts, counts, probs, n, M, rng):
    # Oversample + dedup per row. On heavy-tailed distributions where a few
    # tiers dominate the weight, sampling exactly M per row leads to many dup
    # rows, and each per-call rng.choice(p=probs) to rejection-fix is O(tiers).
    # Oversampling by 2x absorbs the vast majority of dups into one vectorized
    # choice call; the rare short rows still use per-call rejection.
    num_tiers = len(counts)
    draws_per_row = max(int(M * 2), M + 8)
    total = n * draws_per_row
    tiers = rng.choice(num_tiers, size=total, p=probs)
    offsets = (rng.random(total) * counts[tiers]).astype(np.int64)
    cand = (starts[tiers] + offsets).astype(np.uint32).reshape(n, draws_per_row)

    ids = np.empty((n, M), dtype=np.uint32)
    short_rows = []
    for i in range(n):
        _, first_idx = np.unique(cand[i], return_index=True)
        uniq = cand[i][np.sort(first_idx)]
        if len(uniq) >= M:
            ids[i] = uniq[:M]
        else:
            short_rows.append((i, uniq))

    for i, uniq in short_rows:
        seen = set(int(x) for x in uniq)
        while len(seen) < M:
            t = rng.choice(num_tiers, p=probs)
            gid = int(starts[t] + rng.integers(counts[t]))
            seen.add(gid)
        ids[i] = np.fromiter(seen, dtype=np.uint32, count=M)
    return ids


def sample_uniform_chunk(vocab, n, M, rng):
    ids = rng.integers(0, vocab, size=(n, M), dtype=np.uint32)
    sorted_ids = np.sort(ids, axis=1)
    dup_rows = np.where(np.any(sorted_ids[:, 1:] == sorted_ids[:, :-1], axis=1))[0]

    def draw_one(rng):
        return int(rng.integers(vocab))

    _rejection_fix(ids, dup_rows, draw_one, M, rng)
    return ids


def sample_zipfian_chunk(vocab, exponent, n, M, rng):
    # rng.zipf produces 1-indexed samples over [1, ∞); truncate to [1, vocab],
    # shift to [0, vocab), oversample so the post-truncation yield meets M.
    over = max(int(M * 2), M + 32)
    raw = rng.zipf(exponent, size=(n, over))
    mask = raw <= vocab
    ids = np.zeros((n, M), dtype=np.uint32)
    fallback_rows = []
    for i in range(n):
        kept = raw[i][mask[i]] - 1
        _, first = np.unique(kept, return_index=True)
        uniq = kept[np.sort(first)]
        if len(uniq) >= M:
            ids[i] = uniq[:M].astype(np.uint32)
        else:
            fallback_rows.append(i)

    if fallback_rows:
        def draw_one(rng):
            while True:
                x = rng.zipf(exponent)
                if x <= vocab:
                    return int(x - 1)
        _rejection_fix(ids, fallback_rows, draw_one, M, rng)
    return ids


def build_sampler(spec):
    if spec.kind == "csv":
        starts, counts, probs, total = load_csv_tiers(spec.csv_path)
        return (lambda n, M, rng: sample_csv_chunk(starts, counts, probs, n, M, rng)), total
    if spec.kind == "uniform":
        return (lambda n, M, rng: sample_uniform_chunk(spec.vocab, n, M, rng)), spec.vocab
    if spec.kind == "zipfian":
        return (lambda n, M, rng: sample_zipfian_chunk(spec.vocab, spec.exponent, n, M, rng)), spec.vocab
    raise ValueError(f"unknown distribution kind: {spec.kind}")


def _worker(args):
    spec, memmap_path, start_row, end_row, num_cols, seed, worker_id = args
    sampler, _ = build_sampler(spec)
    values = np.lib.format.open_memmap(memmap_path, mode="r+")
    rng = np.random.default_rng(seed + worker_id)
    t0 = time.perf_counter()
    for cs in range(start_row, end_row, CHUNK_ROWS):
        ce = min(cs + CHUNK_ROWS, end_row)
        values[cs:ce] = sampler(ce - cs, num_cols, rng)
    return worker_id, end_row - start_row, time.perf_counter() - t0


def generate(spec: DistSpec, num_rows: int, num_cols: int, output: str,
             workers: int, seed: int):
    _, total_unique = build_sampler(spec)
    print(f"Distribution: {spec.kind}, {total_unique:,} unique items")
    print(f"Generating: N={num_rows:,}, M={num_cols}, workers={workers}, seed={seed}")
    if num_cols > total_unique:
        raise ValueError(f"M={num_cols} exceeds vocab/unique={total_unique}")

    os.makedirs(os.path.dirname(os.path.abspath(output)) or ".", exist_ok=True)

    values = np.lib.format.open_memmap(
        output, mode="w+", dtype=np.uint32, shape=(num_rows, num_cols),
    )
    del values

    rows_per = num_rows // workers
    tasks = []
    for i in range(workers):
        start = i * rows_per
        end = num_rows if i == workers - 1 else start + rows_per
        tasks.append((spec, os.path.abspath(output), start, end, num_cols, seed, i))

    t0 = time.perf_counter()
    with mp.get_context("spawn").Pool(workers) as pool:
        for worker_id, n_done, elapsed in pool.imap_unordered(_worker, tasks):
            print(f"  worker {worker_id}: {n_done:,} rows in {elapsed:.1f}s", flush=True)

    total = time.perf_counter() - t0
    size_mb = os.path.getsize(output) / 1e6
    print(f"Done in {total:.1f}s — {output} ({size_mb:.0f} MB)")


def main():
    parser = argparse.ArgumentParser(
        description="Generate multiset datasets as memmap .npy files",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__.split("\n\n", 1)[1],
    )
    sub = parser.add_subparsers(dest="kind", required=True)

    def add_common(p):
        p.add_argument("--num-rows", type=int, required=True, help="Number of keys (N)")
        p.add_argument("--num-cols", type=int, required=True, help="Values per key (M)")
        p.add_argument("--output", required=True, help="Output .npy path")
        p.add_argument("--workers", type=int, default=max(1, mp.cpu_count() - 1))
        p.add_argument("--seed", type=int, default=DEFAULT_SEED)

    p_csv = sub.add_parser("csv", help="Empirical (metric, count) CSV distribution")
    p_csv.add_argument("csv_path", help="Path to (metric, count) CSV with header row")
    add_common(p_csv)

    p_uni = sub.add_parser("uniform", help="Uniform over [0, vocab)")
    p_uni.add_argument("--vocab", type=int, required=True)
    add_common(p_uni)

    p_zipf = sub.add_parser("zipfian", help="Zipfian with given exponent, truncated to vocab")
    p_zipf.add_argument("--vocab", type=int, required=True)
    p_zipf.add_argument("--exponent", type=float, required=True)
    add_common(p_zipf)

    args = parser.parse_args()

    if args.kind == "csv":
        spec = DistSpec(kind="csv", csv_path=args.csv_path)
    elif args.kind == "uniform":
        spec = DistSpec(kind="uniform", vocab=args.vocab)
    elif args.kind == "zipfian":
        spec = DistSpec(kind="zipfian", vocab=args.vocab, exponent=args.exponent)

    generate(spec, args.num_rows, args.num_cols, args.output, args.workers, args.seed)


if __name__ == "__main__":
    main()
