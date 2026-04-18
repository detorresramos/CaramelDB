"""Run the empirical-distribution grid and render a markdown table.

Grid: 2 datasets × M∈{10,25,50,100} × 4 strategies (Column, Column+Permute,
Column+SharedCB, Column+SharedCB+Permute). N=10K, AutoFilter, global_sort
5 iters, seed=42.
"""

import argparse
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from benchmark_multiset import load_empirical_tiers, run_single

DATASETS = [
    ("asin_clicks", "/Users/david/Downloads/caramel_asin_click_distribution.csv"),
    ("query_freq", "/Users/david/Downloads/caramel_query_frequency_distribution.csv"),
]
M_VALUES = [10, 25, 50, 100]
STRATEGIES = [
    ("Column", {"permutation": "none", "shared_codebook": False}),
    ("Column+Permute", {"permutation": "global_sort", "shared_codebook": False}),
    ("Column+SharedCB", {"permutation": "none", "shared_codebook": True}),
    ("Column+SharedCB+Permute", {"permutation": "global_sort", "shared_codebook": True}),
]
N = 10_000
SEED = 42


def build_config(dataset_name, csv_path, num_cols, strategy_name, strategy_opts, vocab_size):
    return {
        "num_rows": N,
        "num_cols": num_cols,
        "vocab_size": vocab_size,
        "distribution": "empirical",
        "dist_params": {"csv_path": csv_path, "dataset_name": dataset_name},
        "permutation": strategy_opts["permutation"],
        "permutation_params": {"refinement_iterations": 5} if strategy_opts["permutation"] == "global_sort" else {},
        "prefilter": "auto",
        "filter_params": {"fingerprint_bits": 8},
        "shared_codebook": strategy_opts["shared_codebook"],
        "shared_filter": False,
        "strategy_name": strategy_name,
        "dataset_name": dataset_name,
    }


def render_table(results, metric, bold_min=True, fmt=".2f"):
    """Render a markdown table: rows = (dataset, M), cols = strategies."""
    strategies = [s[0] for s in STRATEGIES]
    lines = []
    lines.append("| Dataset | M | " + " | ".join(strategies) + " |")
    lines.append("| :--- | ---: | " + " | ".join(["---:"] * len(strategies)) + " |")
    by_dataset_m = {}
    for r in results:
        by_dataset_m.setdefault((r["dataset_name"], r["num_cols"]), {})[r["strategy_name"]] = r[metric]

    for dataset_name, _ in DATASETS:
        for m in M_VALUES:
            row_vals = [by_dataset_m[(dataset_name, m)][s] for s in strategies]
            best = min(row_vals) if bold_min else None
            cells = []
            for v in row_vals:
                s = f"{v:{fmt}}"
                if bold_min and v == best:
                    s = f"**{s}**"
                cells.append(s)
            lines.append(f"| {dataset_name} | {m} | " + " | ".join(cells) + " |")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-json", default="scripts/multiset_empirical_results.json")
    parser.add_argument("--output-md", default="artifacts/multiset-benchmark-empirical.md")
    args = parser.parse_args()

    dataset_info = {}
    for name, path in DATASETS:
        _, _, _, total_unique = load_empirical_tiers(path)
        dataset_info[name] = {"csv_path": path, "total_unique": total_unique}
        print(f"{name}: {total_unique:,} unique items")

    configs = []
    for dataset_name, csv_path in DATASETS:
        vocab_size = dataset_info[dataset_name]["total_unique"]
        for m in M_VALUES:
            for strategy_name, strategy_opts in STRATEGIES:
                configs.append(build_config(dataset_name, csv_path, m, strategy_name, strategy_opts, vocab_size))

    total = len(configs)
    print(f"\nRunning {total} configurations")
    print("=" * 80)

    results = []
    t0 = time.perf_counter()
    for i, cfg in enumerate(configs):
        elapsed = time.perf_counter() - t0
        eta = elapsed / max(i, 1) * (total - i) if i > 0 else 0
        print(f"\n[{i + 1}/{total}] {cfg['dataset_name']} M={cfg['num_cols']} {cfg['strategy_name']} (ETA: {eta:.0f}s)", flush=True)
        result = run_single(cfg, verbose=True)
        result["dataset_name"] = cfg["dataset_name"]
        result["strategy_name"] = cfg["strategy_name"]
        results.append(result)

        with open(args.output_json, "w") as f:
            json.dump({"dataset_info": dataset_info, "results": results}, f, indent=2)

    print("\n" + "=" * 80)
    print(f"Done in {time.perf_counter() - t0:.0f}s")

    bits_table = render_table(results, "bits_per_key", bold_min=True, fmt=".2f")
    build_table = render_table(results, "total_construction_s", bold_min=True, fmt=".3f")
    query_table = render_table(results, "query_ns", bold_min=True, fmt=".1f")

    md = []
    md.append("# Multiset Benchmark on Empirical Distributions\n")
    md.append("## Datasets\n")
    md.append(
        "- **asin_clicks**: Amazon search ASIN click distribution (one week of US production search traffic, subsampled). "
        f"{dataset_info['asin_clicks']['total_unique']:,} unique ASINs, ~150M total clicks. "
        "Each `(clicks, count)` row means `count` ASINs received exactly `clicks` search clicks."
    )
    md.append(
        "- **query_freq**: mobile keyword-search query frequency distribution over 5 weeks (bot/spam filtered). "
        f"{dataset_info['query_freq']['total_unique']:,} unique queries. "
        "Each `(frequency, count)` row means `count` queries appeared exactly `frequency` times.\n"
    )
    md.append("## Setup\n")
    md.append(f"- **N** (keys): {N:,}")
    md.append("- **M** (values per key): 10, 25, 50, 100")
    md.append("- **Vocab**: full dataset cardinality (see above)")
    md.append(
        "- **Sampling**: per-row, pick `M` distinct item IDs by: sample a tier "
        "with probability ∝ `metric × count`, then uniform-random offset within tier.\n"
    )
    md.append("All strategies use AutoFilter prefilter. Permute variants use global_sort with 5 refinement iterations.\n")
    md.append("## Results\n")
    md.append("### Bits per key (serialized size)\n")
    md.append(bits_table + "\n")
    md.append("### Construction time (seconds, includes permutation)\n")
    md.append(build_table + "\n")
    md.append("### Query latency (ns/query)\n")
    md.append(query_table + "\n")

    os.makedirs(os.path.dirname(args.output_md), exist_ok=True)
    with open(args.output_md, "w") as f:
        f.write("\n".join(md))

    print(f"\nJSON → {args.output_json}")
    print(f"Markdown → {args.output_md}")


if __name__ == "__main__":
    main()
