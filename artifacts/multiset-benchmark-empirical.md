# Multiset Benchmark on Empirical Distributions

## Datasets

- **asin_clicks**: Amazon search ASIN click distribution (one week of US production search traffic, subsampled). 12,000,000 unique ASINs, ~150M total clicks. Each `(clicks, count)` row means `count` ASINs received exactly `clicks` search clicks.
- **query_freq**: mobile keyword-search query frequency distribution over 5 weeks (bot/spam filtered). 792,266,140 unique queries. Each `(frequency, count)` row means `count` queries appeared exactly `frequency` times.

## Setup

- **N** (keys): 10,000
- **M** (values per key): 10, 25, 50, 100
- **Vocab**: full dataset cardinality (see above)
- **Sampling**: per-row, pick `M` distinct item IDs by: sample a tier with probability ∝ `metric × count`, then uniform-random offset within tier.

All strategies use AutoFilter prefilter. Permute variants use global_sort with 5 refinement iterations.

## Baseline

**uint32 flat**: `(M + 1) × 4` bytes per entry — one uint32 key plus M uint32 values, no hash table overhead. Distribution-independent (same for both datasets).

## Results

### Bits per key (serialized size)

| Dataset | M | uint32 baseline | Column | Column+Permute | Column+SharedCB | Column+SharedCB+Permute |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| asin_clicks | 10 | **352** | 467.22 | 452.01 | 475.37 | 475.52 |
| asin_clicks | 25 | **832** | 1168.59 | 1077.14 | 1158.88 | 1158.75 |
| asin_clicks | 50 | **1632** | 2335.67 | 2048.09 | 2230.92 | 2230.95 |
| asin_clicks | 100 | **3232** | 4672.63 | 3826.77 | 4237.51 | 4237.86 |
| query_freq | 10 | **352** | 465.12 | 449.00 | 473.76 | 473.75 |
| query_freq | 25 | **832** | 1163.33 | 1078.34 | 1172.10 | 1172.11 |
| query_freq | 50 | **1632** | 2326.13 | 2087.85 | 2310.55 | 2310.47 |
| query_freq | 100 | **3232** | 4653.23 | 4016.83 | 4547.73 | 4547.11 |

### Construction time (seconds, includes permutation)

| Dataset | M | Column | Column+Permute | Column+SharedCB | Column+SharedCB+Permute |
| :--- | ---: | ---: | ---: | ---: | ---: |
| asin_clicks | 10 | **0.138** | 0.359 | 0.159 | 0.360 |
| asin_clicks | 25 | **0.381** | 1.384 | 0.624 | 1.486 |
| asin_clicks | 50 | **0.837** | 4.092 | 1.205 | 4.041 |
| asin_clicks | 100 | **1.433** | 13.703 | 2.015 | 14.527 |
| query_freq | 10 | **0.152** | 0.359 | 0.181 | 0.378 |
| query_freq | 25 | **0.389** | 1.421 | 0.528 | 1.539 |
| query_freq | 50 | **0.774** | 4.455 | 1.238 | 4.400 |
| query_freq | 100 | **1.548** | 16.102 | 1.980 | 16.952 |

### Query latency (ns/query)

| Dataset | M | Column | Column+Permute | Column+SharedCB | Column+SharedCB+Permute |
| :--- | ---: | ---: | ---: | ---: | ---: |
| asin_clicks | 10 | **337.7** | 347.0 | 362.2 | 407.3 |
| asin_clicks | 25 | **737.9** | 827.8 | 895.3 | 840.2 |
| asin_clicks | 50 | 1819.8 | 1966.6 | 1623.3 | **1585.7** |
| asin_clicks | 100 | 4095.6 | 3576.2 | 3724.6 | **3351.2** |
| query_freq | 10 | 403.1 | 411.6 | **401.3** | 416.8 |
| query_freq | 25 | 867.2 | 830.2 | 855.0 | **789.1** |
| query_freq | 50 | **1722.2** | 1722.6 | 1858.3 | 1824.0 |
| query_freq | 100 | **3342.8** | 3809.9 | 3546.9 | 3562.2 |

---

## N=100M Results (asin_clicks, M=10)

N=100,000,000 keys, M=10 values per key, Vocab=12,000,000 unique ASINs. Values sampled from the ASIN click distribution (one week of US production search traffic, ~150M total clicks). AutoFilter prefilter, global_sort with 5 refinement iterations.

| Strategy | HT baseline | bits/key | Permute (s) | Build (s) | Total (s) | Query (ns) |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| Column | 352 | 264.36 | 0 | 1391.17 | 1391.17 | 71521.3 |
| Column+Permute | 352 | 243.71 | 448.48 | 1319.32 | 1767.80 | 55473.4 |
| Column+SharedCB | 352 | **241.94** | 0 | 1379.95 | 1379.95 | 25463.4 |
| Column+SharedCB+Permute | 352 | 241.95 | 453.49 | 1379.27 | 1832.76 | **18717.5** |
