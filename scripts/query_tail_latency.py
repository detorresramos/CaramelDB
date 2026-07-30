"""Per-query latency distribution for one saved multiset CSF.

Pairs with benchmark_multiset.py, which measures latency in the process that
just did the build by timing batches of 250 keys and recording each batch's
mean. That is fine for a median but cannot produce a tail percentile: averaging
250 keys per sample destroys exactly the outliers a p99 is asking about. This
script times every query individually instead, so the tail survives.

Run it against a saved index rather than inline with a build -- measuring inside
a process still holding the build's allocations inflates latency substantially
(observed 1.1x-2.1x), even with nothing else running on the machine.

Cost of that: two perf_counter_ns() calls sit inside the measured region, worth
roughly 100 ns. Against a ~7 us query that is ~1.3%, and it is identical across
arms, so it shifts every number by the same small amount rather than changing
any comparison. Reported as timer_overhead_ns so it can be subtracted.

Run one at a time, pinned to a single core, with nothing else on the box, using
the interpreter whose carameldb wrote the index.
"""

import argparse
import json
import time

import numpy as np

import carameldb


def estimate_timer_overhead_ns(samples=20000):
    """Cost of the two clock reads that bracket each query."""
    acc = 0
    for _ in range(samples):
        t0 = time.perf_counter_ns()
        acc += time.perf_counter_ns() - t0
    return acc / samples


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--index", required=True)
    p.add_argument("--n", type=int, required=True)
    p.add_argument("--label", required=True)
    p.add_argument("--queries", type=int, default=200000,
                   help="Measured queries. 200k puts ~2000 samples above p99.")
    p.add_argument("--warmup", type=int, default=20000)
    p.add_argument("--seed", type=int, default=42)
    p.add_argument("--output-json", default=None)
    args = p.parse_args()

    t0 = time.perf_counter()
    csf = carameldb.load(args.index)
    load_s = time.perf_counter() - t0

    keys = np.arange(args.n, dtype=np.uint32)
    rng = np.random.RandomState(args.seed)

    warm_idx = rng.choice(len(keys), size=args.warmup, replace=False)
    warm_keys = [keys[i] for i in warm_idx]
    for k in warm_keys:
        csf.query(k)

    overhead_ns = estimate_timer_overhead_ns()

    idx = rng.choice(len(keys), size=args.queries, replace=False)
    query_keys = [keys[i] for i in idx]

    lat = np.empty(args.queries, dtype=np.int64)
    perf = time.perf_counter_ns
    query = csf.query
    for i in range(args.queries):
        k = query_keys[i]
        t = perf()
        query(k)
        lat[i] = perf() - t

    stats = {
        "label": args.label,
        "index": args.index,
        "load_s": round(load_s, 2),
        "queries": int(args.queries),
        "timer_overhead_ns": round(overhead_ns, 1),
        "min_ns": float(np.min(lat)),
        "median_ns": round(float(np.percentile(lat, 50)), 1),
        "mean_ns": round(float(np.mean(lat)), 1),
        "p90_ns": round(float(np.percentile(lat, 90)), 1),
        "p99_ns": round(float(np.percentile(lat, 99)), 1),
        "p999_ns": round(float(np.percentile(lat, 99.9)), 1),
        "max_ns": float(np.max(lat)),
    }
    print(json.dumps(stats, indent=2), flush=True)
    if args.output_json:
        with open(args.output_json, "w") as f:
            json.dump(stats, f, indent=2)


if __name__ == "__main__":
    main()
