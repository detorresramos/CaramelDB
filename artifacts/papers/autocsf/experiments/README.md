# AutoCSF Experiments

Reproducible experiments for the AutoCSF paper. Validates theoretical bounds and compares against Shibuya et al.'s epsilon selection method.

## Setup

From the repo root:

```bash
bin/build.py
pip install numpy matplotlib tqdm
```

## Experiments

### Theory Validation

Validates theoretical lower/upper bounds on bits/key saved against empirical measurements across filter types (XOR, BinaryFuse, Bloom) and value distributions (unique, Zipfian, uniform-100).

```bash
python artifacts/papers/autocsf/experiments/theory_validation/run_experiments.py
python artifacts/papers/autocsf/experiments/theory_validation/make_plots.py
```

Output: `theory_validation/figures/`

### Shibuya Comparison

Head-to-head of our theory-guided Bloom filter parameter selection vs Shibuya et al.'s empirical entropy-based approach, measuring actual bits/key across the full alpha range.

```bash
python artifacts/papers/autocsf/experiments/shibuya_comparison/run_experiments.py
python artifacts/papers/autocsf/experiments/shibuya_comparison/make_plots.py
```

Output: `shibuya_comparison/figures/`

### External Baselines

Comparisons against Sux4J, MPH tables, learned CSFs, and hash tables live in the [CaramelDB-Benchmarks](https://github.com/detorresramos/AutoCSF-Benchmarks) repo.
