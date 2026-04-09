"""Key-to-array benchmark: compare Column, Packed, and RaggedColumn strategies.

Measures construction time, serialized size (bits/key), and query throughput
across fixed-length and ragged arrays, with and without permutation/shared codebook.

Examples:
    # Single config smoke test
    python scripts/benchmark_array.py

    # Full sweep
    python scripts/benchmark_array.py --sweep
"""

import argparse
import json
import os
import tempfile
import time

import numpy as np

import carameldb
from carameldb import (
    AutoFilterConfig,
    GlobalSortPermutationConfig,
)

SEED = 42
NUM_QUERY_KEYS = 250
WARMUP_TRIALS = 3
MEASURE_TRIALS = 10

# ── Data Generation ──────────────────────────────────────────────────────────

def gen_zipfian_probs(vocab_size, exponent):
    ranks = np.arange(1, vocab_size + 1, dtype=np.float64)
    probs = 1.0 / ranks ** exponent
    probs /= probs.sum()
    return probs


def gen_array_data(num_keys, vocab_size, exponent, seed, fixed_length=None,
                   length_range=None):
    """Generate key-to-array data.

    Exactly one of fixed_length or length_range must be provided.
    - fixed_length=M: all rows have length M.
    - length_range=(lo, hi): lengths uniform over [lo, hi].

    Returns (keys, values) where values is a list of lists.
    """
    rng = np.random.default_rng(seed)
    probs = gen_zipfian_probs(vocab_size, exponent)

    if fixed_length is not None:
        lengths = np.full(num_keys, fixed_length, dtype=np.int32)
    else:
        lo, hi = length_range
        lengths = rng.integers(lo, hi + 1, size=num_keys).astype(np.int32)

    keys = [f"key_{i}" for i in range(num_keys)]
    values = []
    for i in range(num_keys):
        L = int(lengths[i])
        L = min(L, vocab_size)
        row = rng.choice(vocab_size, size=L, replace=False, p=probs).tolist()
        values.append(row)

    return keys, values


def pad_to_max(values, pad_value=0):
    """Pad ragged values to rectangular (max length)."""
    max_len = max(len(row) for row in values)
    padded = []
    for row in values:
        padded.append(row + [pad_value] * (max_len - len(row)))
    return np.array(padded, dtype=np.uint32), max_len


# ── Measurement Helpers ──────────────────────────────────────────────────────

def measure_size(csf):
    with tempfile.NamedTemporaryFile(suffix=".csf", delete=False) as tmp:
        tmp_path = tmp.name
    try:
        csf.save(tmp_path)
        return os.path.getsize(tmp_path)
    finally:
        os.unlink(tmp_path)


def measure_query_throughput(csf, keys):
    n_sample = min(NUM_QUERY_KEYS, len(keys))
    rng = np.random.RandomState(SEED)
    trial_ns = []
    for t in range(WARMUP_TRIALS + MEASURE_TRIALS):
        indices = rng.choice(len(keys), size=n_sample, replace=False)
        query_keys = [keys[i] for i in indices]
        start = time.perf_counter()
        for k in query_keys:
            csf.query(k)
        elapsed = time.perf_counter() - start
        if t >= WARMUP_TRIALS:
            trial_ns.append((elapsed / n_sample) * 1e9)
    return float(np.median(trial_ns))


def verify_correctness(csf, keys, values, num_checks=50):
    rng = np.random.RandomState(SEED + 1)
    n_check = min(num_checks, len(keys))
    indices = rng.choice(len(keys), size=n_check, replace=False)
    for idx in indices:
        result = csf.query(keys[idx])
        expected = sorted(values[idx])
        actual = sorted(result)
        if actual != expected:
            raise ValueError(
                f"Correctness check failed for key {keys[idx]}: "
                f"expected {expected}, got {actual}"
            )


# ── Strategy Runners ─────────────────────────────────────────────────────────

def run_column(keys, values_2d, permute=False, shared_cb=False):
    """Column strategy: M independent per-column CSFs (MultisetCsf)."""
    perm_config = GlobalSortPermutationConfig(refinement_iterations=5) if permute else None
    t0 = time.perf_counter()
    csf = carameldb.Caramel(
        keys, values_2d,
        permutation=perm_config,
        prefilter=AutoFilterConfig(),
        shared_codebook=shared_cb,
        verbose=False,
    )
    build_s = time.perf_counter() - t0
    return csf, build_s


def run_packed(keys, values_lol):
    """Packed strategy: single CSF with concatenated Huffman + STOP."""
    t0 = time.perf_counter()
    csf = carameldb.CaramelPacked(keys, values_lol, verbose=False)
    build_s = time.perf_counter() - t0
    return csf, build_s


def run_ragged_column(keys, values_lol, permute=False, shared_cb=False):
    """RaggedColumn strategy: length CSF + per-column CSFs."""
    perm_config = GlobalSortPermutationConfig(refinement_iterations=5) if permute else None
    t0 = time.perf_counter()
    csf = carameldb.CaramelRagged(
        keys, values_lol,
        permutation=perm_config,
        prefilter=AutoFilterConfig(),
        shared_codebook=shared_cb,
        verbose=False,
    )
    build_s = time.perf_counter() - t0
    return csf, build_s


def run_padded_column(keys, values_lol, permute=False):
    """PaddedColumn baseline: pad to max_length, use Column strategy."""
    values_2d, max_len = pad_to_max(values_lol)
    perm_config = GlobalSortPermutationConfig(refinement_iterations=5) if permute else None
    t0 = time.perf_counter()
    csf = carameldb.Caramel(
        keys, values_2d,
        permutation=perm_config,
        prefilter=AutoFilterConfig(),
        verbose=False,
    )
    build_s = time.perf_counter() - t0
    return csf, build_s


# ── Single Configuration Runner ──────────────────────────────────────────────

MAX_BUILD_SECONDS = 30  # Skip strategies that exceed this


def run_strategy(strategy_name, keys, values_lol, values_2d=None):
    """Run a single strategy and return metrics, or None if it times out."""
    try:
        if strategy_name == "Column":
            csf, build_s = run_column(keys, values_2d)
        elif strategy_name == "Column+Permute":
            csf, build_s = run_column(keys, values_2d, permute=True)
        elif strategy_name == "Column+SharedCB":
            csf, build_s = run_column(keys, values_2d, shared_cb=True)
        elif strategy_name == "Column+SharedCB+Permute":
            csf, build_s = run_column(keys, values_2d, permute=True, shared_cb=True)
        elif strategy_name == "Packed":
            csf, build_s = run_packed(keys, values_lol)
        elif strategy_name == "RaggedColumn":
            csf, build_s = run_ragged_column(keys, values_lol)
        elif strategy_name == "RaggedColumn+Permute":
            csf, build_s = run_ragged_column(keys, values_lol, permute=True)
        elif strategy_name == "PaddedColumn":
            csf, build_s = run_padded_column(keys, values_lol)
        elif strategy_name == "PaddedColumn+Permute":
            csf, build_s = run_padded_column(keys, values_lol, permute=True)
        else:
            raise ValueError(f"Unknown strategy: {strategy_name}")
    except Exception as e:
        return {"build_s": -1, "bits_per_key": -1, "query_ns": -1, "size_bytes": -1, "error": str(e)}

    # Verify correctness (use original ragged values for Packed/Ragged)
    if strategy_name.startswith("Column") or strategy_name.startswith("Padded"):
        if values_2d is not None:
            check_vals = [row.tolist() for row in values_2d]
        else:
            check_vals = values_lol
    else:
        check_vals = values_lol

    if not strategy_name.startswith("Padded"):
        verify_correctness(csf, keys, check_vals)

    size_bytes = measure_size(csf)
    bits_per_key = (size_bytes * 8) / len(keys)
    query_ns = measure_query_throughput(csf, keys)

    return {
        "build_s": round(build_s, 4),
        "bits_per_key": round(bits_per_key, 2),
        "query_ns": round(query_ns, 1),
        "size_bytes": size_bytes,
    }


# ── Sweep Configurations ─────────────────────────────────────────────────────

FIXED_STRATEGIES = [
    "Column", "Column+Permute", "Column+SharedCB",
    "Column+SharedCB+Permute", "Packed",
]
RAGGED_STRATEGIES = [
    "RaggedColumn", "RaggedColumn+Permute",
    "PaddedColumn", "PaddedColumn+Permute", "Packed",
]

DEFAULT_NUM_KEYS = 10_000
DEFAULT_FIXED_LENGTHS = [10, 25]
DEFAULT_LENGTH_RANGES = [(5, 15), (20, 40)]
DEFAULT_VOCAB_SIZES = [100, 1000]
DEFAULT_EXPONENTS = [0.5, 1.0, 2.0]


def build_sweep_configs():
    """Build all (data config, strategy set) pairs."""
    configs = []
    # Fixed-length configs
    for M in DEFAULT_FIXED_LENGTHS:
        for vocab in DEFAULT_VOCAB_SIZES:
            if M >= vocab:
                continue
            for exp in DEFAULT_EXPONENTS:
                configs.append({
                    "fixed_length": M,
                    "length_range": None,
                    "vocab_size": vocab,
                    "exponent": exp,
                    "is_ragged": False,
                })
    # Ragged configs
    for lo, hi in DEFAULT_LENGTH_RANGES:
        for vocab in DEFAULT_VOCAB_SIZES:
            if hi >= vocab:
                continue
            for exp in DEFAULT_EXPONENTS:
                configs.append({
                    "fixed_length": None,
                    "length_range": (lo, hi),
                    "vocab_size": vocab,
                    "exponent": exp,
                    "is_ragged": True,
                })
    return configs


# ── Output Formatting ─────────────────────────────────────────────────────────

def _metric_val(strategies_dict, strategy_name, metric):
    m = strategies_dict.get(strategy_name, {})
    v = m.get(metric, "-")
    if v == -1:
        return "-"
    return v


def _fmt(val, is_best):
    if val == "-":
        return "-"
    s = str(val)
    return f"**{s}**" if is_best else s


def _make_metric_tables(section_title, data_rows, data_headers, strategies, metric_labels):
    """Create one table per metric, bolding the best value per row."""
    lines = [f"\n### {section_title}\n"]
    for metric, metric_title in metric_labels:
        headers = data_headers + strategies
        lines.append(f"\n#### {metric_title}\n")
        lines.append("| " + " | ".join(headers) + " |")
        lines.append("| " + " | ".join(["---:" for _ in headers]) + " |")

        for r in data_rows:
            vals = [_metric_val(r["strategies"], s, metric) for s in strategies]
            numeric = [(i, v) for i, v in enumerate(vals) if isinstance(v, (int, float))]
            best_idx = min(numeric, key=lambda x: x[1])[0] if numeric else -1
            row = [str(x) for x in r["data_cols"]]
            row += [_fmt(v, i == best_idx) for i, v in enumerate(vals)]
            lines.append("| " + " | ".join(row) + " |")

    return "\n".join(lines)


def format_results_tables(all_results):
    lines = []

    metric_labels = [
        ("bits_per_key", "Bits per key (serialized size)"),
        ("build_s", "Construction time (seconds)"),
        ("query_ns", "Query latency (ns/query)"),
    ]

    # Group results by data type
    fixed_results = [r for r in all_results if not r["is_ragged"]]
    ragged_results = [r for r in all_results if r["is_ragged"]]

    if fixed_results:
        data_rows = [{"data_cols": [r["fixed_length"], r["vocab_size"], r["exponent"]],
                      "strategies": r["strategies"]} for r in fixed_results]
        lines.append(_make_metric_tables(
            "Fixed-Length Arrays", data_rows,
            ["M", "Vocab", "s"], FIXED_STRATEGIES, metric_labels))

    if ragged_results:
        data_rows = [{"data_cols": [f"[{r['length_range'][0]}, {r['length_range'][1]}]",
                                    r["vocab_size"], r["exponent"]],
                      "strategies": r["strategies"]} for r in ragged_results]
        lines.append(_make_metric_tables(
            "Ragged Arrays", data_rows,
            ["Length Range", "Vocab", "s"], RAGGED_STRATEGIES, metric_labels))

    return "\n".join(lines) + "\n"


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Key-to-array benchmark")
    parser.add_argument("--num-keys", type=int, default=DEFAULT_NUM_KEYS)
    parser.add_argument("--sweep", action="store_true", help="Run full sweep")
    parser.add_argument("--output", type=str, default=None)
    args = parser.parse_args()

    num_keys = args.num_keys

    if args.sweep:
        data_configs = build_sweep_configs()
    else:
        data_configs = [
            {"fixed_length": 10, "length_range": None, "vocab_size": 100, "exponent": 1.5, "is_ragged": False},
            {"fixed_length": None, "length_range": (5, 15), "vocab_size": 100, "exponent": 1.5, "is_ragged": True},
        ]

    total_work = 0
    for dc in data_configs:
        if dc["is_ragged"]:
            total_work += len(RAGGED_STRATEGIES)
        else:
            total_work += len(FIXED_STRATEGIES)

    print(f"Key-to-Array Benchmark ({len(data_configs)} data configs, {total_work} total strategy runs)")
    print("=" * 80)

    all_results = []
    t_start = time.perf_counter()
    work_done = 0

    for dc_idx, dc in enumerate(data_configs):
        vocab = dc["vocab_size"]
        exp = dc["exponent"]
        is_ragged = dc["is_ragged"]

        if is_ragged:
            lo, hi = dc["length_range"]
            label = f"lengths=[{lo},{hi}], Vocab={vocab}, s={exp}"
        else:
            label = f"M={dc['fixed_length']}, Vocab={vocab}, s={exp}"
        print(f"\n[{dc_idx+1}/{len(data_configs)}] {label}")

        # Generate data
        keys, values_lol = gen_array_data(
            num_keys, vocab, exp, SEED,
            fixed_length=dc["fixed_length"],
            length_range=dc["length_range"],
        )
        lengths = [len(row) for row in values_lol]
        print(f"  lengths: min={min(lengths)}, max={max(lengths)}, "
              f"mean={np.mean(lengths):.1f}, std={np.std(lengths):.1f}")

        # For fixed-length (Column) strategies, need 2D numpy array
        values_2d = None
        if not is_ragged:
            values_2d = np.array(values_lol, dtype=np.uint32)

        strategies = RAGGED_STRATEGIES if is_ragged else FIXED_STRATEGIES

        result = {
            "fixed_length": dc["fixed_length"],
            "length_range": dc["length_range"],
            "vocab_size": vocab,
            "exponent": exp, "is_ragged": is_ragged,
            "strategies": {},
        }
        for strat in strategies:
            elapsed = time.perf_counter() - t_start
            work_done += 1
            if work_done > 1:
                eta = elapsed / (work_done - 1) * (total_work - work_done + 1)
                print(f"  {strat} (ETA: {eta:.0f}s)...", end="", flush=True)
            else:
                print(f"  {strat}...", end="", flush=True)
            metrics = run_strategy(strat, keys, values_lol, values_2d)
            result["strategies"][strat] = metrics
            if "error" in metrics:
                print(f" SKIP ({metrics['error'][:60]})")
            else:
                print(f" {metrics['bits_per_key']} bits/key, {metrics['build_s']}s, {metrics['query_ns']}ns")
        all_results.append(result)

    # Print tables to stdout
    tables = format_results_tables(all_results)
    print("\n" + tables)

    # Save raw JSON
    json_path = os.path.join(os.path.dirname(__file__), "..", "artifacts", "array-benchmark.json")
    if args.output:
        json_path = args.output
    with open(json_path, "w") as f:
        json.dump(all_results, f, indent=2)
    print(f"\nRaw data saved to {json_path}")


if __name__ == "__main__":
    main()
