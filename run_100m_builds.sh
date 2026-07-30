#!/usr/bin/env bash
# Builds all 4 100M-row CSFs. Pair 1 = movielens (both perms in parallel),
# pair 2 = amazon (both perms in parallel). Query timing from these runs is
# thrown away — actual query numbers come from QueryLatencyBenchmark.

set -euo pipefail
cd "$(dirname "$0")"

PY=cython/.venv/bin/python
BENCH=scripts/benchmark_multiset.py
OUT=results/fresh
DATA=$HOME/recsys-datasets
LOG=$OUT/build_100m.log

mkdir -p "$OUT"
: > "$LOG"

build() {
    local dataset="$1" perm="$2"
    local tag="${dataset}_$( [ "$perm" = "none" ] && echo nopermute || echo permute )"
    rm -rf "$OUT/${tag}"
    echo "[$(date -Is)] START $tag" >> "$LOG"
    "$PY" "$BENCH" \
        --npy "$DATA/${dataset}.npy" \
        --permutation "$perm" \
        --query-count 100 --query-warmup 20 \
        --save-dir "$OUT/${tag}" \
        --output-json "$OUT/${tag}.json" \
        --quiet >> "$LOG" 2>&1
    echo "[$(date -Is)] END   $tag" >> "$LOG"
}

echo "[$(date -Is)] === Pair 1: movielens_100m × 2 in parallel ===" >> "$LOG"
build movielens_100m none &
PID_A=$!
build movielens_100m global_sort &
PID_B=$!
wait $PID_A $PID_B

echo "[$(date -Is)] === Pair 2: amazon_books_100m × 2 in parallel ===" >> "$LOG"
build amazon_books_100m none &
PID_C=$!
build amazon_books_100m global_sort &
PID_D=$!
wait $PID_C $PID_D

echo "[$(date -Is)] 100M BUILDS COMPLETE" >> "$LOG"
