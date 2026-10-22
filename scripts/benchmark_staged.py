"""Build/query/size benchmark matrix used to check that the staged construction
pipeline keeps performance. Runs every case `--reps` times, each in a fresh
subprocess so peak RSS is per run, and reports the median of each metric.
Query time within a run is the fastest of QUERY_ROUNDS passes: the median
of a single-threaded query loop swung up to 2x between identical runs. Uses only the public
`carameldb.Caramel` API, so the same script runs against any checkout:

    <python> scripts/benchmark_staged.py --out results/staged_<label>.json
    <python> scripts/benchmark_staged.py --compare base.json new.json
"""

import argparse
import json
import os
import resource
import shutil
import statistics
import subprocess
import sys
import tempfile
import time

import numpy as np

QUERY_SAMPLE = 200_000
QUERY_ROUNDS = 10


def make_values(dist, n, rng, shape=None):
    size = (n,) if shape is None else (n, shape)
    if dist == "uniform16":
        return rng.integers(0, 16, size=size, dtype=np.uint32)
    if dist == "uniform64k":
        return rng.integers(0, 1 << 16, size=size, dtype=np.uint32)
    if dist == "zipf":
        return np.minimum(rng.zipf(1.3, size=size), (1 << 31) - 1).astype(np.uint32)
    if dist == "unique":
        if shape is None:
            return rng.permutation(n).astype(np.uint32)
        return rng.integers(0, 1 << 32, size=size, dtype=np.uint64).astype(np.uint32)
    if dist == "skew90":
        v = rng.integers(1, 1000, size=size, dtype=np.uint32)
        v[rng.random(size) < 0.9] = 0
        return v
    raise ValueError(dist)


def entropy_bits(v):
    _, counts = np.unique(v, return_counts=True)
    p = counts / counts.sum()
    return float(-(p * np.log2(p)).sum())


def make_filter(name):
    import carameldb

    if name == "none":
        return None
    if name == "auto":
        return carameldb.AutoFilterConfig()
    if name == "bloom":
        return carameldb.BloomFilterConfig(8, 3)
    if name == "xor":
        return carameldb.XORFilterConfig(8)
    if name == "binfuse":
        return carameldb.BinaryFuseFilterConfig(8)
    raise ValueError(name)


def path_size(path):
    if os.path.isdir(path):
        return sum(os.path.getsize(os.path.join(path, f)) for f in os.listdir(path))
    return os.path.getsize(path)


def footprint():
    """(current, peak since the previous call) physical footprint in bytes.

    macOS ru_maxrss counts pages malloc has already marked reusable, so it
    overstates peak memory by up to several x; the kernel's phys_footprint is
    what the OS charges the process. Elsewhere fall back to ru_maxrss.
    """
    if sys.platform != "darwin":
        peak = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024
        return peak, peak
    import ctypes

    buf = (ctypes.c_uint64 * 64)()
    libc = ctypes.CDLL("libproc.dylib")
    if libc.proc_pid_rusage(os.getpid(), 4, ctypes.byref(buf)) != 0:  # RUSAGE_INFO_V4
        raise OSError("proc_pid_rusage failed")
    words = list(buf)[2:]  # skip the 16-byte uuid
    return words[7], words[33]  # ri_phys_footprint, ri_interval_max_phys_footprint


def run_case(case):
    import carameldb

    rng = np.random.default_rng(case.get("seed", 0))
    n = case["n"]
    kind = case["kind"]
    keys = [f"key{i}" for i in range(n)]
    if kind == "csf":
        values = make_values(case["dist"], n, rng)
    elif kind == "multiset":
        values = make_values(case["dist"], n, rng, shape=case["m"])
    else:  # ragged / packed
        lengths = rng.integers(1, case["m"] + 1, size=n)
        flat = make_values(case["dist"], int(lengths.sum()), rng)
        offsets = np.concatenate([[0], np.cumsum(lengths)])
        values = [flat[offsets[i]:offsets[i + 1]].tolist() for i in range(n)]
    ent = entropy_bits(values if kind in ("csf", "multiset") else flat)

    kwargs = dict(verbose=False, prefilter=make_filter(case.get("filter", "none")))
    if case.get("shared_codebook"):
        kwargs["shared_codebook"] = True
    if case.get("shared_filter"):
        kwargs["shared_filter"] = True
    if kind == "packed":
        kwargs = dict(verbose=False, strategy="packed")

    rss_before = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    fp_before, _ = footprint()  # also resets the interval peak
    t0 = time.perf_counter()
    csf = carameldb.Caramel(keys, values, **kwargs)
    build_s = time.perf_counter() - t0
    _, fp_peak = footprint()
    rss_after = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    scale = 1 if sys.platform == "darwin" else 1024  # ru_maxrss: bytes on macOS

    tmp = tempfile.mkdtemp()
    try:
        path = os.path.join(tmp, "csf")
        csf.save(path)
        size = path_size(path)
    finally:
        shutil.rmtree(tmp)

    sample = rng.choice(n, size=min(n, QUERY_SAMPLE), replace=False)
    qkeys = [keys[i] for i in sample]
    if kind == "csf":
        query = lambda: csf.query_batch(qkeys)
    else:
        query = lambda: [csf.query(k) for k in qkeys]
    times = []
    for _ in range(QUERY_ROUNDS):
        t0 = time.perf_counter()
        out = query()
        times.append(time.perf_counter() - t0)
    query_s = min(times)
    if kind == "csf":
        ok = bool(np.array_equal(out, values[sample]))
    elif kind == "multiset":
        ok = bool(np.array_equal(np.array(out, dtype=np.uint32), values[sample]))
    elif kind == "packed":
        ok = all(sorted(o) == sorted(values[i]) for o, i in zip(out, sample))
    else:
        ok = all(list(o) == values[i] for o, i in zip(out, sample))

    return dict(
        case=case,
        entropy_bits=ent,
        build_s=build_s,
        build_peak_mb=max(0, fp_peak - fp_before) / 1e6,
        peak_footprint_mb=fp_peak / 1e6,
        rss_delta_mb=max(0, rss_after - rss_before) * scale / 1e6,
        size_bytes=size,
        bits_per_key=size * 8 / n,
        query_ns=query_s / len(qkeys) * 1e9,
        correct=ok,
    )


def cases(quick):
    out = []
    ns = [100_000, 1_000_000] if quick else [100_000, 1_000_000, 10_000_000]
    for n in ns:
        for dist in ["uniform16", "uniform64k", "zipf", "unique", "skew90"]:
            for flt in ["none", "auto"]:
                out.append(dict(kind="csf", n=n, dist=dist, filter=flt))
    for n in [100_000, 1_000_000] + ([] if quick else [4_000_000]):
        for m in [8, 32]:
            if n == 4_000_000 and m == 32:
                continue
            for dist in ["uniform16", "zipf", "skew90", "unique"]:
                for flt in ["none", "auto"]:
                    out.append(dict(kind="multiset", n=n, m=m, dist=dist, filter=flt))
    out.append(dict(kind="multiset", n=1_000_000, m=16, dist="zipf", filter="none", shared_codebook=True))
    out.append(dict(kind="multiset", n=1_000_000, m=16, dist="skew90", filter="auto", shared_filter=True))
    out.append(dict(kind="multiset", n=1_000_000, m=16, dist="skew90", filter="auto", shared_codebook=True, shared_filter=True))
    for n in [100_000, 1_000_000]:
        out.append(dict(kind="ragged", n=n, m=16, dist="zipf", filter="none"))
        out.append(dict(kind="ragged", n=n, m=16, dist="skew90", filter="auto"))
        out.append(dict(kind="packed", n=n, m=10, dist="zipf"))
    return out


def label(case):
    keys = ["kind", "n", "m", "dist", "filter", "shared_codebook", "shared_filter"]
    return " ".join(f"{case[k]}" if k not in ("shared_codebook", "shared_filter") else k
                    for k in keys if k in case and case[k] is not False)


def compare(base_path, new_path):
    base = {label(r["case"]): r for r in json.load(open(base_path))}
    new = {label(r["case"]): r for r in json.load(open(new_path))}
    print(f"{'case':58s} {'H':>5s} {'build':>14s} {'peakMB':>14s} {'bits/key':>16s} {'query ns':>14s} ok")
    for k, b in base.items():
        if k not in new:
            continue
        r = new[k]

        def pair(f, fmt):
            return f"{format(b[f], fmt)}→{format(r[f], fmt)}"

        print(f"{k:58s} {b['entropy_bits']:5.1f} {pair('build_s', '.2f'):>14s} "
              f"{pair('build_peak_mb', '.0f'):>14s} {pair('bits_per_key', '.2f'):>16s} "
              f"{pair('query_ns', '.0f'):>14s} {'Y' if b['correct'] else 'N'}→{'Y' if r['correct'] else 'N'}")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--out")
    p.add_argument("--quick", action="store_true")
    p.add_argument("--case", help=argparse.SUPPRESS)
    p.add_argument("--compare", nargs=2)
    p.add_argument("--filter", default="", help="only run cases whose label contains this")
    p.add_argument("--reps", type=int, default=3, help="fresh-process runs per case")
    args = p.parse_args()

    if args.compare:
        compare(*args.compare)
        return
    if args.case:
        print(json.dumps(run_case(json.loads(args.case))))
        return

    results = []
    for case in cases(args.quick):
        if args.filter not in label(case):
            continue
        runs = []
        # The 32M-cell cases dominate runtime; one run each keeps the matrix
        # under an hour per checkout.
        reps = 1 if case["n"] * case.get("m", 1) >= 16_000_000 else args.reps
        for _ in range(reps):
            proc = subprocess.run([sys.executable, __file__, "--case", json.dumps(case)],
                                  capture_output=True, text=True)
            if proc.returncode != 0:
                print(f"FAILED {label(case)}\n{proc.stderr[-2000:]}", flush=True)
                break
            runs.append(json.loads(proc.stdout.strip().splitlines()[-1]))
        if not runs:
            continue
        r = dict(runs[0])
        for metric in ["build_s", "build_peak_mb", "peak_footprint_mb", "rss_delta_mb", "query_ns"]:
            r[metric] = statistics.median(x[metric] for x in runs)
            r[metric + "_runs"] = [x[metric] for x in runs]
        r["correct"] = all(x["correct"] for x in runs)
        r["reps"] = len(runs)
        results.append(r)
        print(f"{label(case):58s} build={r['build_s']:.2f}s peak={r['build_peak_mb']:.0f}MB "
              f"bits/key={r['bits_per_key']:.2f} q={r['query_ns']:.0f}ns ok={r['correct']}",
              flush=True)
        if args.out:
            json.dump(results, open(args.out, "w"), indent=1)


if __name__ == "__main__":
    main()
