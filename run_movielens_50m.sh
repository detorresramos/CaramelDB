#!/usr/bin/env bash
# Builds movielens_50m x {no-permute, global_sort} in parallel and records
# size / build time / query latency for each. Artifacts go to results/rerun.

set -euo pipefail
cd "$(dirname "$0")"

PY=cython/.venv/bin/python
BENCH=scripts/benchmark_multiset.py
OUT=results/rerun
DATA=$HOME/recsys-datasets
LOG=$OUT/movielens_50m.log

mkdir -p "$OUT"
: > "$LOG"

build() {
    local perm="$1"
    local tag="movielens_50m_$( [ "$perm" = "none" ] && echo nopermute || echo permute )"
    rm -rf "$OUT/${tag}"
    echo "[$(date -Is)] START $tag" >> "$LOG"
    "$PY" "$BENCH" \
        --npy "$DATA/movielens_50m.npy" \
        --permutation "$perm" \
        --save-dir "$OUT/${tag}" \
        --output-json "$OUT/${tag}.json" \
        --quiet >> "$LOG" 2>&1
    echo "[$(date -Is)] END   $tag" >> "$LOG"
}

build none &
PID_NP=$!
build global_sort &
PID_P=$!

wait $PID_NP $PID_P
echo "[$(date -Is)] MOVIELENS 50M BUILDS COMPLETE" >> "$LOG"
