"""N=100M experiments with 10 refinement iterations.

Runs each experiment as a subprocess to guarantee memory cleanup between runs.
"""

import json
import os
import subprocess
import sys
import time


EXPERIMENTS = [
    ("query_freq", "/Users/david/Downloads/caramel_query_frequency_distribution.csv", 10),
    ("asin_clicks", "/Users/david/Downloads/caramel_asin_click_distribution.csv", 25),
    ("query_freq", "/Users/david/Downloads/caramel_query_frequency_distribution.csv", 25),
]

OUTPUT_JSON = "scripts/multiset_100m_batch_results.json"

# Single-experiment worker script run as subprocess
WORKER = '''
import gc, json, os, sys, tempfile, time
import numpy as np
sys.path.insert(0, os.path.dirname("{script_dir}") + "/scripts" if "{script_dir}" else "scripts")
from benchmark_multiset import (
    MEASURE_TRIALS, SEED, WARMUP_TRIALS,
    load_empirical_tiers, make_permutation_config,
    measure_query_throughput, sample_empirical, verify_correctness,
)
import carameldb
from carameldb import AutoFilterConfig

N = 100_000_000
REFINEMENT_ITERATIONS = 10
STRATEGIES = [
    ("Column", "none", False),
    ("Column+Permute", "global_sort", False),
    ("Column+SharedCB", "none", True),
    ("Column+SharedCB+Permute", "global_sort", True),
]

dataset_name = "{dataset_name}"
csv_path = "{csv_path}"
M = {M}
output_path = "{output_path}"

tier_starts, tier_counts, tier_probs, total_unique = load_empirical_tiers(csv_path)
print(f"Dataset: {{dataset_name}}, {{total_unique:,}} unique items, N={{N:,}}, M={{M}}", flush=True)

print("Generating keys...", flush=True)
t0 = time.perf_counter()
keys = [f"q{{i}}" for i in range(N)]
print(f"  keys: {{time.perf_counter() - t0:.1f}}s", flush=True)

print("Sampling values...", flush=True)
t0 = time.perf_counter()
rng = np.random.default_rng(SEED)
values = sample_empirical(tier_starts, tier_counts, tier_probs, N, M, rng)
print(f"  values: {{time.perf_counter() - t0:.1f}}s, shape={{values.shape}}", flush=True)

results = []
for name, perm, shared_cb in STRATEGIES:
    print(f"\\n--- {{name}} ---", flush=True)
    vals = values.copy()

    perm_config = make_permutation_config(perm, {{"refinement_iterations": REFINEMENT_ITERATIONS}})
    if perm_config is not None:
        print("  Permuting...", flush=True)
        t0 = time.perf_counter()
        carameldb.permute_uint32(vals, config=perm_config)
        perm_time = time.perf_counter() - t0
        print(f"  Permute: {{perm_time:.2f}}s", flush=True)
    else:
        perm_time = 0.0

    print("  Building CSF...", flush=True)
    t0 = time.perf_counter()
    csf = carameldb.Caramel(keys, vals, permutation=None, prefilter=AutoFilterConfig(),
                            shared_codebook=shared_cb, verbose=False)
    build_time = time.perf_counter() - t0
    print(f"  Build: {{build_time:.2f}}s", flush=True)

    with tempfile.NamedTemporaryFile(suffix=".csf", delete=False) as tmp:
        tmp_path = tmp.name
    try:
        csf.save(tmp_path)
        size_bytes = os.path.getsize(tmp_path)
    finally:
        os.unlink(tmp_path)
    bits_per_key = (size_bytes * 8) / N
    print(f"  Size: {{size_bytes:,}} bytes = {{bits_per_key:.2f}} bits/key", flush=True)

    print("  Measuring query latency...", flush=True)
    query_ns = measure_query_throughput(csf, keys, WARMUP_TRIALS, MEASURE_TRIALS)
    print(f"  Query: {{query_ns:.1f}} ns/query", flush=True)

    print("  Verifying correctness...", flush=True)
    verify_correctness(csf, keys, vals)
    print("  OK", flush=True)

    results.append({{
        "dataset": dataset_name, "strategy": name, "N": N, "M": M,
        "vocab": total_unique, "refinement_iterations": REFINEMENT_ITERATIONS,
        "permutation_s": round(perm_time, 2), "build_s": round(build_time, 2),
        "total_construction_s": round(perm_time + build_time, 2),
        "size_bytes": size_bytes, "bits_per_key": round(bits_per_key, 4),
        "query_ns": round(query_ns, 1),
    }})
    print(f"  Summary: perm={{perm_time:.2f}}s  build={{build_time:.2f}}s  "
          f"total={{perm_time+build_time:.2f}}s  {{bits_per_key:.2f}} bits/key  {{query_ns:.1f}} ns/q", flush=True)
    del csf, vals; gc.collect()

with open(output_path, "w") as f:
    json.dump(results, f, indent=2)
print(f"\\nSaved {{len(results)}} results to {{output_path}}", flush=True)
'''


def main():
    all_results = []
    t_global = time.perf_counter()
    script_dir = os.path.dirname(os.path.abspath(__file__))

    for i, (dataset_name, csv_path, M) in enumerate(EXPERIMENTS):
        print(f"\n{'='*80}")
        print(f"Experiment {i+1}/{len(EXPERIMENTS)}: {dataset_name} M={M}")
        print("=" * 80, flush=True)

        tmp_json = f"/tmp/multiset_100m_exp_{i}.json"
        worker_code = WORKER.format(
            script_dir=script_dir,
            dataset_name=dataset_name,
            csv_path=csv_path,
            M=M,
            output_path=tmp_json,
        )

        result = subprocess.run(
            [sys.executable, "-c", worker_code],
            cwd=os.path.dirname(script_dir),
            timeout=36000,
        )

        if result.returncode != 0:
            print(f"  FAILED with exit code {result.returncode}", flush=True)
            continue

        with open(tmp_json) as f:
            exp_results = json.load(f)
        all_results.extend(exp_results)

        with open(OUTPUT_JSON, "w") as f:
            json.dump(all_results, f, indent=2)
        print(f"  Accumulated {len(all_results)} results so far", flush=True)

    elapsed = time.perf_counter() - t_global
    print(f"\n{'='*80}")
    print(f"All done in {elapsed:.0f}s ({elapsed/3600:.1f}h)")
    print(f"Results → {OUTPUT_JSON}")


if __name__ == "__main__":
    main()
