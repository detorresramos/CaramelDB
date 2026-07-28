#!/bin/bash
# Sweep the per-column bucket count (via the CARAMEL_* build knobs) and record
# build time / size / peak RSS from the Python harness plus C++ query latency.
set -e
NPY=${NPY:-datasets/movielens_1m_m100.npy}
N=${N:-1000000}
OUT=${OUT:-results/sweep_buckets}
mkdir -p "$OUT"

run() {
  local name=$1; shift
  echo "=== $name ($*) ==="
  env "$@" cython/.venv/bin/python scripts/benchmark_multiset.py \
    --npy "$NPY" --permutation global_sort --refinement-iterations 10 \
    --prefilter auto --save-dir "results/csfs/$name" \
    --output-json "$OUT/$name.json" --quiet --query-trials 5 \
    --correctness-checks 20 2>&1 | tail -9
  CARAMEL_STATS=1 ./build/QueryBench "results/csfs/$name" "$N" 4000 15
}

for spec in "$@"; do
  name=${spec%%:*}
  envs=${spec#*:}
  run "$name" $envs
done
