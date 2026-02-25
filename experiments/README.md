# Experiments

Reproducing all figures:

```bash
bash experiments/run_all.sh
```

Or run each experiment individually:

## Paper Plots

Validates theoretical bounds (lower/upper) against empirical measurements across
filter types (XOR, BinaryFuse, Bloom) and value distributions.

```bash
python experiments/paper_plots/run_experiments.py   # generate data
python experiments/paper_plots/make_plots.py        # generate figures
```

Output: `paper_plots/figures/`

## Baselines

Compares CSF+filter against hash tables, Java implementations, and Shibuya's
epsilon selection strategy.

```bash
python experiments/baselines/run_baselines.py       # generate data
python experiments/baselines/make_plots.py          # generate figures and tables
```

Output: `baselines/figures/`

## Shibuya Comparison

Direct comparison of our theory-guided Bloom filter parameters vs Shibuya et al.
(WABI 2021).

```bash
python experiments/shibuya_comparison/make_plots.py
```

Output: `shibuya_comparison/figures/`
