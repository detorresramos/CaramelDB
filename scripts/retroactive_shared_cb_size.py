"""Retroactively correct Column+SharedCB sizes in empirical_grid_results.json.

Background: the MultisetCSF save format serialized the shared codebook once per
column file (cereal's shared_ptr tracking doesn't cross archives, and each column
is a separate archive). This duplication caused `shared_codebook=True` runs to
report larger bits/key than `shared_codebook=False`, the opposite of what the
design intends.

The fix factors the shared codebook into a single file at the multiset level.
The savings vs. the buggy format are exactly (M - 1) * serialized_codebook_bytes
— bit arrays, filters, chunk metadata, and per-column seeds are byte-for-byte
identical. We can compute `serialized_codebook_bytes` from the raw .npy dataset
without building a CSF (just a pooled frequency histogram + canonical Huffman).

Usage:
    python scripts/retroactive_shared_cb_size.py \
        --dataset-dir datasets/ \
        --input-json scripts/empirical_grid_results.json \
        --output-json scripts/empirical_grid_results_corrected.json
"""

import argparse
import json
import os
import time

import numpy as np

import carameldb

# Keep in sync with run_empirical_grid.py
DATASET_PREFIXES = {
    "asin_clicks": "asin",
    "query_freq": "query",
}


def npy_path(dataset_dir, dataset_name, M):
    prefix = DATASET_PREFIXES[dataset_name]
    return os.path.join(dataset_dir, f"{prefix}_100m_m{M}.npy")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset-dir", default="datasets/")
    parser.add_argument("--input-json", default="scripts/empirical_grid_results.json")
    parser.add_argument("--output-json",
                        default="scripts/empirical_grid_results_corrected.json")
    args = parser.parse_args()

    with open(args.input_json) as f:
        results = json.load(f)

    shared_rows = [r for r in results if r["strategy"] == "Column+SharedCB"]
    if not shared_rows:
        print("No Column+SharedCB rows found; nothing to correct.")
        return

    # Measure codebook size once per (dataset, M).
    codebook_bytes_cache = {}
    for r in shared_rows:
        key = (r["dataset"], r["M"])
        if key in codebook_bytes_cache:
            continue
        path = npy_path(args.dataset_dir, r["dataset"], r["M"])
        if not os.path.exists(path):
            print(f"SKIP: {path} not found")
            continue
        print(f"Measuring codebook size for {r['dataset']} M={r['M']} ({path})...",
              flush=True)
        t0 = time.perf_counter()
        values = np.load(path, mmap_mode="r")
        cb_bytes = carameldb.shared_codebook_serialized_bytes(values)
        elapsed = time.perf_counter() - t0
        print(f"  codebook = {cb_bytes:,} bytes ({cb_bytes / 1e6:.1f} MB), "
              f"took {elapsed:.1f}s", flush=True)
        codebook_bytes_cache[key] = cb_bytes

    corrected = []
    summary_rows = []
    for r in results:
        if r["strategy"] != "Column+SharedCB":
            corrected.append(r)
            continue
        key = (r["dataset"], r["M"])
        cb_bytes = codebook_bytes_cache.get(key)
        if cb_bytes is None:
            corrected.append(r)
            continue
        savings = (r["M"] - 1) * cb_bytes
        new_size = r["size_bytes"] - savings
        new_bpk = (new_size * 8) / r["N"]
        r2 = dict(r)
        r2["size_bytes"] = new_size
        r2["bits_per_key"] = round(new_bpk, 2)
        r2["codebook_bytes"] = cb_bytes
        r2["savings_bytes"] = savings
        corrected.append(r2)
        summary_rows.append((r["dataset"], r["M"], r["bits_per_key"],
                             round(new_bpk, 2), round(r["bits_per_key"] - new_bpk, 2)))

    with open(args.output_json, "w") as f:
        json.dump(corrected, f, indent=2)

    print("\nCorrection summary:")
    print(f"{'Dataset':<14} {'M':>4} {'Current bpk':>12} {'Corrected bpk':>14} {'Δ bpk':>10}")
    print("-" * 60)
    for ds, M, cur, new, delta in summary_rows:
        print(f"{ds:<14} {M:>4} {cur:>12.2f} {new:>14.2f} {delta:>10.2f}")
    print(f"\nWrote corrected results to {args.output_json}")


if __name__ == "__main__":
    main()
