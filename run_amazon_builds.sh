#!/usr/bin/env bash
# Builds amazon_books_50m × {no-permute, permute} in parallel.
# Query timing from these runs is thrown away — only the saved CSF matters;
# actual query numbers come from QueryLatencyBenchmark.

set -euo pipefail
cd "$(dirname "$0")"

PY=cython/.venv/bin/python
BENCH=scripts/benchmark_multiset.py
OUT=results/fresh
DATA=$HOME/recsys-datasets
LOG=$OUT/amazon_builds.log

mkdir -p "$OUT"
: > "$LOG"

build() {
    local perm="$1"
    local tag="amazon_books_50m_$( [ "$perm" = "none" ] && echo nopermute || echo permute )"
    rm -rf "$OUT/${tag}"
    echo "[$(date -Is)] START $tag" >> "$LOG"
    "$PY" "$BENCH" \
        --npy "$DATA/amazon_books_50m.npy" \
        --permutation "$perm" \
        --query-count 100 --query-warmup 20 \
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
echo "[$(date -Is)] AMAZON BUILDS COMPLETE" >> "$LOG"
