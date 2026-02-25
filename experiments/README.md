# Experiments

Empirical validation of CaramelDB's prefilter optimization theory and comparison
against baselines.

## Setup

Build CaramelDB and install Python bindings (from the repo root):

```bash
bin/build.py
pip install -r experiments/requirements.txt
```

## Reproducing All Figures

```bash
bash experiments/run_all.sh
```

This runs all three experiment suites below and writes figures to their
respective `figures/` directories. The baselines suite (which sweeps over
N up to 10M) is the slowest.

## Experiment Suites

### Paper Plots

Validates that the theoretical lower and upper bounds on bits/key saved
match empirical measurements, across filter types (XOR, BinaryFuse, Bloom)
and minority-value distributions (unique, Zipfian, uniform-100).

```bash
python experiments/paper_plots/run_experiments.py   # generate data
python experiments/paper_plots/make_plots.py        # generate figures
```

Output: `paper_plots/figures/`

### Baselines

Compares CSF+filter memory and query performance against hash tables (Python
and C++) and Java implementations (Sux4J CSF, MPH table). Also compares our
theory-guided epsilon selection against Shibuya et al. (WABI 2021).

```bash
python experiments/baselines/run_baselines.py       # generate data
python experiments/baselines/make_plots.py          # generate figures and tables
```

Output: `baselines/figures/`

### Shibuya Comparison

Direct head-to-head of our theory-guided Bloom filter parameter selection vs
Shibuya et al.'s empirical entropy-based approach, measuring actual bits/key
across the full alpha range.

```bash
python experiments/shibuya_comparison/make_plots.py
```

Output: `shibuya_comparison/figures/`
