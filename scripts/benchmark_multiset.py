"""Build a Caramel CSF from a pre-generated .npy dataset, measure it, save it.

Pairs with generate_empirical_dataset.py. Takes a row-major 2D uint32 .npy
file (N rows x M cols; each row is the multiset of values for one key) and
reports construction time, serialized size, query latency, and correctness.

Usage:
    python scripts/run_caramel.py --npy datasets/asin_100m_m10.npy \\
        --permutation global_sort --shared-codebook \\
        --save-dir results/asin_m10_permute_sharedcb \\
        --output-json results/asin_m10_permute_sharedcb.json

Keys are synthetic (np.arange(N, dtype=uint32)). If your use case needs real
keys, swap out the `keys = ...` line below.
"""

import argparse
import gc
import json
import os
import shutil
import tempfile
import time

import numpy as np

import carameldb
from carameldb import (
    AutoFilterConfig,
    EntropyPermutationConfig,
    GlobalSortPermutationConfig,
)


def build_permutation(kind: str, refinement_iterations: int):
    if kind == "none":
        return None
    if kind == "global_sort":
        return GlobalSortPermutationConfig(refinement_iterations=refinement_iterations)
    if kind == "entropy":
        return EntropyPermutationConfig()
    raise ValueError(f"Unknown permutation: {kind!r}")


def build_prefilter(kind: str):
    if kind == "auto":
        return AutoFilterConfig()
    if kind == "none":
        return None
    raise ValueError(f"Unknown prefilter: {kind!r}")


def dir_size_bytes(path: str) -> int:
    return sum(
        os.path.getsize(os.path.join(path, f)) for f in os.listdir(path)
    )


def measure_query_ns(csf, keys, num_sample: int, warmup: int, measured: int, seed: int) -> float:
    """Median ns/query over `measured` trials, each querying a fresh random
    sample of `num_sample` keys. Warmup trials are discarded."""
    n_sample = min(num_sample, len(keys))
    rng = np.random.RandomState(seed)
    trial_ns = []
    for t in range(warmup + measured):
        indices = rng.choice(len(keys), size=n_sample, replace=False)
        query_keys = [keys[i] for i in indices]
        start = time.perf_counter()
        for k in query_keys:
            csf.query(k)
        elapsed = time.perf_counter() - start
        if t >= warmup:
            trial_ns.append((elapsed / n_sample) * 1e9)
    return float(np.median(trial_ns))


def verify_correctness(csf, keys, values, num_checks: int, seed: int):
    rng = np.random.RandomState(seed)
    n = min(num_checks, len(keys))
    indices = rng.choice(len(keys), size=n, replace=False)
    for idx in indices:
        got = sorted(csf.query(keys[idx]))
        want = sorted(values[idx].tolist())
        if got != want:
            raise AssertionError(
                f"Correctness check failed for key index {idx}: "
                f"expected {want}, got {got}"
            )


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--npy", required=True,
                   help="Path to pre-generated 2D uint32 .npy (N rows x M cols)")
    p.add_argument("--permutation", default="none",
                   choices=["none", "global_sort", "entropy"])
    p.add_argument("--refinement-iterations", type=int, default=10,
                   help="Iterations for global_sort permutation (ignored otherwise)")
    p.add_argument("--shared-codebook", action="store_true",
                   help="Pool all columns into a single shared Huffman codebook")
    p.add_argument("--shared-filter", action="store_true",
                   help="Group columns with the same MCV under a shared prefilter")
    p.add_argument("--prefilter", default="auto", choices=["auto", "none"])
    p.add_argument("--save-dir", default=None,
                   help="Persist the CSF here. If omitted, CSF is written to a "
                        "temp dir, measured, and deleted.")
    p.add_argument("--output-json", default=None,
                   help="Write metrics JSON here (default: print to stdout only)")
    p.add_argument("--query-sample", type=int, default=250,
                   help="Keys per trial for query latency (default 250)")
    p.add_argument("--query-warmup", type=int, default=3)
    p.add_argument("--query-trials", type=int, default=10)
    p.add_argument("--correctness-checks", type=int, default=50,
                   help="Random keys to spot-check after build (0 to skip)")
    p.add_argument("--seed", type=int, default=42)
    p.add_argument("--quiet", action="store_true", help="Disable Caramel's verbose build logs")
    args = p.parse_args()

    # Use mmap so we don't materialize the full array up front. If we permute
    # in place we need copy-on-write so the .npy on disk isn't mutated.
    mmap_mode = "c" if args.permutation != "none" else "r"
    print(f"Loading {args.npy} (mmap mode={mmap_mode!r})...", flush=True)
    values = np.load(args.npy, mmap_mode=mmap_mode)
    if values.ndim != 2:
        raise ValueError(f"Expected 2D array, got shape {values.shape}")
    N, M = values.shape
    print(f"  N={N:,} rows, M={M} cols, dtype={values.dtype}", flush=True)

    keys = np.arange(N, dtype=np.uint32)

    permutation = build_permutation(args.permutation, args.refinement_iterations)
    prefilter = build_prefilter(args.prefilter)

    print(
        f"Building: permutation={args.permutation}, "
        f"shared_codebook={args.shared_codebook}, "
        f"shared_filter={args.shared_filter}, prefilter={args.prefilter}",
        flush=True,
    )
    t0 = time.perf_counter()
    csf = carameldb.Caramel(
        keys, values,
        permutation=permutation,
        prefilter=prefilter,
        shared_codebook=args.shared_codebook,
        shared_filter=args.shared_filter,
        verbose=not args.quiet,
    )
    wall_s = time.perf_counter() - t0

    perm_s = getattr(csf, "permutation_seconds", 0.0)
    build_s = getattr(csf, "build_seconds", 0.0)

    # Save and measure size. If save_dir is given, keep the artifacts; else
    # use a temp dir.
    if args.save_dir is not None:
        save_path = args.save_dir
        if os.path.exists(save_path):
            shutil.rmtree(save_path)
        os.makedirs(os.path.dirname(os.path.abspath(save_path)) or ".", exist_ok=True)
        csf.save(save_path)
        size_bytes = dir_size_bytes(save_path)
        print(f"Saved CSF to {save_path}", flush=True)
    else:
        tmp_path = tempfile.mkdtemp(suffix=".csf")
        shutil.rmtree(tmp_path)
        try:
            csf.save(tmp_path)
            size_bytes = dir_size_bytes(tmp_path)
        finally:
            shutil.rmtree(tmp_path, ignore_errors=True)
    bits_per_key = (size_bytes * 8) / N

    # Correctness. If we permuted in place, `values` is the permuted copy, so
    # re-load read-only for verification against ground truth.
    if args.correctness_checks > 0:
        print(f"Verifying {args.correctness_checks} random keys...", flush=True)
        verify_values = (
            values if args.permutation == "none"
            else np.load(args.npy, mmap_mode="r")
        )
        verify_correctness(csf, keys, verify_values,
                           args.correctness_checks, args.seed + 1)

    print(f"Measuring query latency "
          f"({args.query_warmup} warmup + {args.query_trials} trials × "
          f"{args.query_sample} keys)...", flush=True)
    query_ns = measure_query_ns(
        csf, keys,
        args.query_sample, args.query_warmup, args.query_trials, args.seed,
    )

    metrics = {
        "npy": os.path.abspath(args.npy),
        "N": int(N),
        "M": int(M),
        "permutation": args.permutation,
        "refinement_iterations": (
            args.refinement_iterations if args.permutation == "global_sort" else None
        ),
        "shared_codebook": args.shared_codebook,
        "shared_filter": args.shared_filter,
        "prefilter": args.prefilter,
        "permutation_s": round(perm_s, 3),
        "build_s": round(build_s, 3),
        "wall_s": round(wall_s, 3),
        "size_bytes": int(size_bytes),
        "bits_per_key": round(bits_per_key, 3),
        "query_ns_median": round(query_ns, 1),
        "save_dir": os.path.abspath(args.save_dir) if args.save_dir else None,
    }

    print("\n=== Results ===")
    print(f"N×M              : {N:,} × {M}")
    print(f"Permutation time : {perm_s:.2f} s")
    print(f"Build time       : {build_s:.2f} s")
    print(f"Wall time        : {wall_s:.2f} s")
    print(f"Serialized size  : {size_bytes:,} bytes  ({bits_per_key:.2f} bits/key)")
    print(f"Query latency    : {query_ns:.1f} ns/query (median)")

    if args.output_json:
        os.makedirs(os.path.dirname(os.path.abspath(args.output_json)) or ".", exist_ok=True)
        with open(args.output_json, "w") as f:
            json.dump(metrics, f, indent=2)
        print(f"Metrics → {args.output_json}")

    del csf, keys, values
    gc.collect()


if __name__ == "__main__":
    main()
