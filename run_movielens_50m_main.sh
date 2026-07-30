#!/usr/bin/env bash
# A/B baseline: same dataset, same benchmark script, same 2-in-parallel protocol
# as run_movielens_50m.sh, but against carameldb built from the `main` branch in
# a separate worktree. Only the library differs between the two runs.

set -euo pipefail
cd "$(dirname "$0")"

WT=/tmp/claude-1001/-home-caramel-CaramelDB/f909d02e-88a5-4438-a1ed-c009d1124c46/scratchpad/main-baseline
PY=$WT/cython/.venv/bin/python
BENCH=$WT/scripts/benchmark_multiset.py   # copy of this branch's script
OUT=results/rerun-main
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
echo "[$(date -Is)] MAIN BASELINE BUILDS COMPLETE" >> "$LOG"
