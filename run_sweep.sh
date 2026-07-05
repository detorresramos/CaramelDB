#!/usr/bin/env bash
# Fresh benchmark sweep: 4 datasets × {no permutation, global_sort permutation}.
# Both variants use shared_codebook=False, prefilter=auto.

set -euo pipefail
cd "$(dirname "$0")"

PY=cython/.venv/bin/python
BENCH=scripts/benchmark_multiset.py
OUT=results/fresh
LOG=$OUT/sweep.log
DATA=$HOME/recsys-datasets

mkdir -p "$OUT"
: > "$LOG"

run() {
    local name="$1" npy="$2" perm="$3"
    local tag="${name}_$( [ "$perm" = "none" ] && echo nopermute || echo permute )"
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

# Small datasets first, so failures surface quickly.
run movielens_50m   "$DATA/movielens_50m.npy"    none
run movielens_50m   "$DATA/movielens_50m.npy"    global_sort
run amazon_books_50m  "$DATA/amazon_books_50m.npy"  none
run amazon_books_50m  "$DATA/amazon_books_50m.npy"  global_sort
run movielens_100m  "$DATA/movielens_100m.npy"   none
run movielens_100m  "$DATA/movielens_100m.npy"   global_sort
run amazon_books_100m "$DATA/amazon_books_100m.npy" none
run amazon_books_100m "$DATA/amazon_books_100m.npy" global_sort

echo "=== [$(date -Is)] SWEEP COMPLETE ===" | tee -a "$LOG"
