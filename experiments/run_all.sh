#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== Paper Plots ==="
python "$DIR/paper_plots/run_experiments.py"
python "$DIR/paper_plots/make_plots.py"

echo "=== Baselines ==="
python "$DIR/baselines/run_baselines.py"
python "$DIR/baselines/make_plots.py"

echo "=== Shibuya Comparison ==="
python "$DIR/shibuya_comparison/make_plots.py"

echo "Done. Figures written to:"
echo "  $DIR/paper_plots/figures/"
echo "  $DIR/baselines/figures/"
echo "  $DIR/shibuya_comparison/figures/"
