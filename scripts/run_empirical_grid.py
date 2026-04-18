"""Run the full empirical grid search: 2 datasets × 4 M values × 4 strategies.

Loads pre-generated .npy datasets, runs each experiment sequentially,
saves results incrementally, and renders markdown tables.

Usage:
    python scripts/run_empirical_grid.py --dataset-dir datasets/ --output-md artifacts/multiset-benchmark-empirical.md
"""

import argparse
import gc
import json
import os
import sys
import tempfile
import time

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from benchmark_multiset import MEASURE_TRIALS, SEED, WARMUP_TRIALS, measure_query_throughput, verify_correctness

import carameldb
from carameldb import AutoFilterConfig, GlobalSortPermutationConfig

N = 100_000_000
M_VALUES = [10, 25, 50, 100]
REFINEMENT_ITERATIONS = 10

DATASETS = [
    ("asin_clicks", "asin"),
    ("query_freq", "query"),
]

STRATEGIES = [
    ("Column",          None,                                                            False),
    ("Column+Permute",  GlobalSortPermutationConfig(refinement_iterations=REFINEMENT_ITERATIONS), False),
    ("Column+SharedCB", None,                                                            True),
]

OUTPUT_JSON = "scripts/empirical_grid_results.json"


def run_one(dataset_name, M, strategy_name, permutation, shared_codebook, npy_path):
    print(f"  Loading {npy_path}...", flush=True)
    values = np.load(npy_path)
    assert values.shape == (N, M), f"Expected ({N}, {M}), got {values.shape}"

    keys = [f"q{i}" for i in range(N)]

    print(f"  Building {strategy_name}...", flush=True)
    t0 = time.perf_counter()
    csf = carameldb.Caramel(
        keys, values,
        permutation=permutation,
        prefilter=AutoFilterConfig(),
        shared_codebook=shared_codebook,
        verbose=True,
    )
    wall_time = time.perf_counter() - t0

    perm_s = csf.permutation_seconds
    build_s = csf.build_seconds

    # Serialized size
    with tempfile.NamedTemporaryFile(suffix=".csf", delete=False) as tmp:
        tmp_path = tmp.name
    try:
        csf.save(tmp_path)
        size_bytes = os.path.getsize(tmp_path)
    finally:
        os.unlink(tmp_path)
    bits_per_key = (size_bytes * 8) / N

    # Query latency
    print(f"  Measuring query latency...", flush=True)
    query_ns = measure_query_throughput(csf, keys, WARMUP_TRIALS, MEASURE_TRIALS)

    # Correctness
    print(f"  Verifying correctness...", flush=True)
    verify_correctness(csf, keys, values if permutation is None else np.load(npy_path))

    print(f"  {strategy_name}: perm={perm_s:.2f}s build={build_s:.2f}s "
          f"wall={wall_time:.2f}s {bits_per_key:.2f} bits/key {query_ns:.1f} ns/q", flush=True)

    del csf, keys, values
    gc.collect()

    return {
        "dataset": dataset_name,
        "M": M,
        "strategy": strategy_name,
        "N": N,
        "permutation_s": round(perm_s, 2),
        "build_s": round(build_s, 2),
        "wall_s": round(wall_time, 2),
        "size_bytes": size_bytes,
        "bits_per_key": round(bits_per_key, 2),
        "query_ns": round(query_ns, 1),
    }


def render_table(results, metric, title, fmt=".2f", bold_min=True, extra_col=None, extra_val=None):
    """Render markdown table: rows = (dataset, M), cols = strategies."""
    strat_names = [s[0] for s in STRATEGIES]
    lines = []
    lines.append(f"### {title}\n")

    header_cols = ["Dataset", "M"]
    if extra_col:
        header_cols.append(extra_col)
    header_cols.extend(strat_names)
    lines.append("| " + " | ".join(header_cols) + " |")
    lines.append("| :--- | ---: | " + " | ".join(["---:"] * (len(header_cols) - 2)) + " |")

    by_key = {}
    for r in results:
        by_key.setdefault((r["dataset"], r["M"]), {})[r["strategy"]] = r[metric]

    for ds_name, ds_prefix in DATASETS:
        for m in M_VALUES:
            key = (ds_name, m)
            if key not in by_key:
                continue
            row_vals = [by_key[key].get(s, None) for s in strat_names]
            valid = [v for v in row_vals if v is not None]
            best = min(valid) if bold_min and valid else None

            cells = [ds_name, str(m)]
            if extra_col:
                cells.append(str(extra_val(m)) if extra_val else "")
            for v in row_vals:
                if v is None:
                    cells.append("—")
                else:
                    s = f"{v:{fmt}}"
                    if bold_min and v == best:
                        s = f"**{s}**"
                    cells.append(s)
            lines.append("| " + " | ".join(cells) + " |")

    return "\n".join(lines)


def render_construction_table(results):
    """Construction table with Permute (s) and Build (s) columns per strategy."""
    strat_names = [s[0] for s in STRATEGIES]
    lines = []
    lines.append("### Construction time (seconds)\n")

    # Build sub-columns: each strategy gets Permute | Build | Total
    header = "| Dataset | M |"
    sep = "| :--- | ---: |"
    for s in strat_names:
        header += f" {s} Perm | {s} Build | {s} Total |"
        sep += " ---: | ---: | ---: |"
    lines.append(header)
    lines.append(sep)

    by_key = {}
    for r in results:
        by_key.setdefault((r["dataset"], r["M"]), {})[r["strategy"]] = r

    for ds_name, ds_prefix in DATASETS:
        for m in M_VALUES:
            key = (ds_name, m)
            if key not in by_key:
                continue
            row = f"| {ds_name} | {m} |"
            for sn in strat_names:
                r = by_key[key].get(sn)
                if r:
                    total = r["permutation_s"] + r["build_s"]
                    row += f" {r['permutation_s']:.1f} | {r['build_s']:.1f} | {total:.1f} |"
                else:
                    row += " — | — | — |"
            lines.append(row)

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset-dir", default="datasets/")
    parser.add_argument("--output-json", default=OUTPUT_JSON)
    parser.add_argument("--output-md", default="artifacts/multiset-benchmark-empirical.md")
    parser.add_argument("--m-order", type=int, nargs="+", default=None,
                        help="Order of M values to run (default: 10 25 50 100)")
    args = parser.parse_args()

    m_order = args.m_order or M_VALUES

    # Load existing results to skip completed configs
    all_results = []
    completed = set()
    if os.path.exists(args.output_json):
        with open(args.output_json) as f:
            all_results = json.load(f)
        for r in all_results:
            completed.add((r["dataset"], r["M"], r["strategy"]))
        print(f"Loaded {len(all_results)} existing results, skipping {len(completed)} configs", flush=True)

    # Build run list: iterate M values in specified order, both datasets per M
    run_list = []
    for m in m_order:
        for ds_name, ds_prefix in DATASETS:
            npy_path = os.path.join(args.dataset_dir, f"{ds_prefix}_100m_m{m}.npy")
            for strat_name, perm, shared_cb in STRATEGIES:
                if (ds_name, m, strat_name) in completed:
                    continue
                run_list.append((ds_name, ds_prefix, m, strat_name, perm, shared_cb, npy_path))

    total = len(run_list)
    print(f"{total} configs to run", flush=True)
    t_global = time.perf_counter()

    for i, (ds_name, ds_prefix, m, strat_name, perm, shared_cb, npy_path) in enumerate(run_list):
        if not os.path.exists(npy_path):
            print(f"SKIP: {npy_path} not found", flush=True)
            continue

        elapsed = time.perf_counter() - t_global
        print(f"\n[{i+1}/{total}] {ds_name} M={m} {strat_name} ({elapsed:.0f}s elapsed)", flush=True)

        result = run_one(ds_name, m, strat_name, perm, shared_cb, npy_path)
        all_results.append(result)

        with open(args.output_json, "w") as f:
            json.dump(all_results, f, indent=2)

    elapsed = time.perf_counter() - t_global
    print(f"\n{'='*80}")
    print(f"All done in {elapsed:.0f}s ({elapsed/3600:.1f}h)")

    # Render markdown
    baseline_fn = lambda m: (m + 1) * 32

    bits_table = render_table(all_results, "bits_per_key", "Bits per key (serialized size)",
                              fmt=".2f", bold_min=True,
                              extra_col="HT baseline", extra_val=baseline_fn)
    construction_table = render_construction_table(all_results)
    query_table = render_table(all_results, "query_ns", "Query latency (ns/query)",
                               fmt=".1f", bold_min=True)

    md_lines = [
        "# Multiset Benchmark on Empirical Distributions (N=100M)\n",
        "## Datasets\n",
        "- **asin_clicks**: ASIN click distribution — 12,000,000 unique ASINs, ~150M total clicks (one week US production search traffic).",
        "- **query_freq**: query frequency distribution — 792,266,140 unique queries over 5 weeks (mobile keyword search, bot/spam filtered).\n",
        "## Setup\n",
        f"- **N**: {N:,}",
        "- **M** (values per key): 10, 25, 50, 100",
        "- **Vocab**: full dataset cardinality",
        "- **Sampling**: weighted without replacement from empirical (metric, count) distribution",
        f"- **Permute**: global_sort, {REFINEMENT_ITERATIONS} refinement iterations",
        "- **Prefilter**: AutoFilter\n",
        "## Results\n",
        bits_table + "\n",
        construction_table + "\n",
        query_table + "\n",
    ]

    with open(args.output_md, "w") as f:
        f.write("\n".join(md_lines))

    print(f"\nJSON → {args.output_json}")
    print(f"Markdown → {args.output_md}")


if __name__ == "__main__":
    main()
