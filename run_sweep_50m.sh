#!/usr/bin/env bash
# Fresh 50M benchmark sweep with arena prefetch: 2 datasets × {no-permute, permute}.
# Overwrites results/fresh/ artifacts.

set -euo pipefail
cd "$(dirname "$0")"

PY=cython/.venv/bin/python
BENCH=scripts/benchmark_multiset.py
OUT=results/fresh
LOG=$OUT/sweep_50m.log
DATA=$HOME/recsys-datasets

mkdir -p "$OUT"
: > "$LOG"

run() {
    local name="$1" npy="$2" perm="$3"
    local tag="${name}_$( [ "$perm" = "none" ] && echo nopermute || echo permute )"
    # Clear old CSF dir since --save-dir refuses to overwrite non-empty targets.
    rm -rf "$OUT/${tag}"
    echo "=== [$(date -Is)] START $tag ===" | tee -a "$LOG"
    "$PY" "$BENCH" \
        --npy "$npy" \
        --permutation "$perm" \
        --query-count 1000 \
        --query-warmup 200 \
        --save-dir "$OUT/${tag}" \
        --output-json "$OUT/${tag}.json" \
        --quiet 2>&1 | tee -a "$LOG"
    echo "=== [$(date -Is)] END   $tag ===" | tee -a "$LOG"
}

run movielens_50m    "$DATA/movielens_50m.npy"    none
run movielens_50m    "$DATA/movielens_50m.npy"    global_sort
run amazon_books_50m "$DATA/amazon_books_50m.npy" none
run amazon_books_50m "$DATA/amazon_books_50m.npy" global_sort

echo "=== [$(date -Is)] 50M SWEEP COMPLETE ===" | tee -a "$LOG"
