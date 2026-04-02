# Multiset CSF Synthetic Benchmark

Testing permutation strategies vs shared codebook on various distributions

Assume we have a mapping of N keys to M unordered values. Each value drawn from vocabulary of size |S| with some underlying distribution. Assume values within each row can be freely permuted or that order doesn't matter.

We'll create an array of M compressed static functions (CSFs), one per column. Each CSF maps keys to values with space proportional to column entropy.

**Data generation:** Each row samples M items without replacement from a Zipfian distribution over |S| items. Zipf exponent s controls skew (0.5 = mild, 2.0 = extreme). 

We'll test the different permutation algorithms (none, entropy (initial algorithm) and global_sort (new algorithm)) on datasets constructed from different underlying distributions. We'll also see how using the shared codebook compares to using permutations. 

## Results

### M=10 columns, N=10,000 keys

| Vocab | Zipf s | Permutation | Shared CB | Perm (s) | Build (s) | bits/key | H_before | H_after |
|------:|-------:|:------------|:----------|--------:|---------:|--------:|--------:|-------:|
| 1,000 | 0.5 | none | no | 0.00 | 0.10 | 142.05 | 95.60 | 95.60 |
| 1,000 | 0.5 | none | yes | 0.00 | 0.10 | 113.45 | 95.60 | 95.60 |
| 1,000 | 0.5 | global_sort | no | 0.01 | 0.09 | 116.81 | 95.60 | 79.11 |
| 1,000 | 0.5 | global_sort | yes | 0.01 | 0.10 | 113.53 | 95.60 | 79.11 |
| 1,000 | 0.5 | entropy | no | 30.50 | 0.60 | 115.50 | 95.60 | 77.41 |
| 1,000 | 0.5 | entropy | yes | 25.40 | 0.10 | 113.44 | 95.60 | 77.41 |
| 1,000 | 1.0 | none | no | 0.00 | 0.08 | 120.34 | 78.56 | 78.56 |
| 1,000 | 1.0 | none | yes | 0.00 | 0.07 | 94.24 | 78.56 | 78.56 |
| 1,000 | 1.0 | global_sort | no | 0.01 | 0.07 | 95.12 | 78.56 | 60.09 |
| 1,000 | 1.0 | global_sort | yes | 0.01 | 0.08 | 92.87 | 78.56 | 60.09 |
| 1,000 | 1.0 | entropy | no | 70.45 | 0.51 | 92.97 | 78.56 | 58.77 |
| 1,000 | 1.0 | entropy | yes | 18.26 | 0.09 | 92.80 | 78.56 | 58.77 |
| 1,000 | 1.5 | none | no | 0.00 | 0.06 | 86.18 | 58.48 | 58.48 |
| 1,000 | 1.5 | none | yes | 0.00 | 0.07 | 73.03 | 58.48 | 58.48 |
| 1,000 | 1.5 | global_sort | no | 0.01 | 0.05 | 59.78 | 58.48 | 37.05 |
| 1,000 | 1.5 | global_sort | yes | 0.01 | 0.06 | 65.71 | 58.48 | 37.05 |
| 1,000 | 1.5 | entropy | no | 15.47 | 0.34 | 59.28 | 58.48 | 36.57 |
| 1,000 | 1.5 | entropy | yes | 4.03 | 0.05 | 65.71 | 58.48 | 36.57 |
| 1,000 | 2.0 | none | no | 0.00 | 0.06 | 62.72 | 46.13 | 46.13 |
| 1,000 | 2.0 | none | yes | 0.00 | 0.06 | 60.46 | 46.13 | 46.13 |
| 1,000 | 2.0 | global_sort | no | 0.01 | 0.03 | 37.79 | 46.13 | 23.85 |
| 1,000 | 2.0 | global_sort | yes | 0.01 | 0.05 | 50.14 | 46.13 | 23.85 |
| 1,000 | 2.0 | entropy | no | 2.71 | 0.20 | 37.63 | 46.13 | 23.65 |
| 1,000 | 2.0 | entropy | yes | 0.69 | 0.04 | 50.15 | 46.13 | 23.65 |
| 10,000 | 0.5 | none | no | 0.00 | 0.94 | 318.60 | 121.05 | 121.05 |
| 10,000 | 0.5 | none | yes | 0.00 | 0.12 | 178.70 | 121.05 | 121.05 |
| 10,000 | 0.5 | global_sort | no | 0.14 | 0.81 | 210.81 | 121.05 | 105.56 |
| 10,000 | 0.5 | global_sort | yes | 0.15 | 0.92 | 178.76 | 121.05 | 105.56 |
| 10,000 | 0.5 | entropy | no | 228.86 | 0.74 | 197.33 | 121.05 | 102.14 |
| 10,000 | 0.5 | entropy | yes | 57.78 | 0.11 | 178.69 | 121.05 | 102.14 |
| 10,000 | 1.0 | none | no | 0.00 | 0.80 | 202.41 | 93.61 | 93.61 |
| 10,000 | 1.0 | none | yes | 0.00 | 0.73 | 140.99 | 93.61 | 93.61 |
| 10,000 | 1.0 | global_sort | no | 0.10 | 0.77 | 156.83 | 93.61 | 76.19 |
| 10,000 | 1.0 | global_sort | yes | 0.11 | 0.72 | 141.01 | 93.61 | 76.19 |
| 10,000 | 1.0 | entropy | no | 118.99 | 0.63 | 147.51 | 93.61 | 74.06 |
| 10,000 | 1.0 | entropy | yes | 30.42 | 0.09 | 140.96 | 93.61 | 74.06 |
| 10,000 | 1.5 | none | no | 0.00 | 0.56 | 99.25 | 61.16 | 61.16 |
| 10,000 | 1.5 | none | yes | 0.00 | 0.57 | 84.63 | 61.16 | 61.16 |
| 10,000 | 1.5 | global_sort | no | 0.06 | 0.39 | 72.56 | 61.16 | 40.12 |
| 10,000 | 1.5 | global_sort | yes | 0.06 | 0.53 | 77.44 | 61.16 | 40.12 |
| 10,000 | 1.5 | entropy | no | 17.60 | 0.36 | 71.07 | 61.16 | 39.38 |
| 10,000 | 1.5 | entropy | yes | 4.61 | 0.06 | 77.44 | 61.16 | 39.38 |
| 10,000 | 2.0 | none | no | 0.00 | 0.41 | 64.12 | 46.46 | 46.46 |
| 10,000 | 2.0 | none | yes | 0.00 | 0.40 | 61.74 | 46.46 | 46.46 |
| 10,000 | 2.0 | global_sort | no | 0.04 | 0.27 | 39.50 | 46.46 | 24.46 |
| 10,000 | 2.0 | global_sort | yes | 0.04 | 0.37 | 51.55 | 46.46 | 24.46 |
| 10,000 | 2.0 | entropy | no | 2.92 | 0.28 | 38.92 | 46.46 | 23.99 |
| 10,000 | 2.0 | entropy | yes | 0.73 | 0.05 | 54.86 | 46.46 | 23.99 |

### M=50 columns, N=10,000 keys

Entropy permutation omitted (too slow: O(N * M * |S|) per round).

| Vocab | Zipf s | Permutation | Shared CB | Perm (s) | Build (s) | bits/key | H_before | H_after |
|------:|-------:|:------------|:----------|--------:|---------:|--------:|--------:|-------:|
| 1,000 | 0.5 | none | no | 0.00 | 3.44 | 712.58 | 480.20 | 480.20 |
| 1,000 | 0.5 | none | yes | 0.00 | 3.92 | 556.95 | 480.20 | 480.20 |
| 1,000 | 0.5 | global_sort | no | 0.05 | 0.34 | 430.21 | 480.20 | 300.82 |
| 1,000 | 0.5 | global_sort | yes | 0.06 | 0.50 | 557.10 | 480.20 | 300.82 |
| 1,000 | 1.0 | none | no | 0.00 | 0.56 | 647.92 | 429.14 | 429.14 |
| 1,000 | 1.0 | none | yes | 0.00 | 0.56 | 503.04 | 429.14 | 429.14 |
| 1,000 | 1.0 | global_sort | no | 0.05 | 0.32 | 353.66 | 429.14 | 235.30 |
| 1,000 | 1.0 | global_sort | yes | 0.04 | 0.48 | 470.11 | 429.14 | 235.30 |
| 1,000 | 1.5 | none | no | 0.00 | 0.51 | 552.10 | 372.45 | 372.45 |
| 1,000 | 1.5 | none | yes | 0.00 | 0.49 | 451.09 | 372.45 | 372.45 |
| 1,000 | 1.5 | global_sort | no | 0.04 | 0.26 | 271.92 | 372.45 | 168.73 |
| 1,000 | 1.5 | global_sort | yes | 0.04 | 0.44 | 388.53 | 372.45 | 168.73 |
| 1,000 | 2.0 | none | no | 0.00 | 0.42 | 462.97 | 328.75 | 328.75 |
| 1,000 | 2.0 | none | yes | 0.00 | 0.44 | 415.19 | 328.75 | 328.75 |
| 1,000 | 2.0 | global_sort | no | 0.03 | 0.19 | 209.55 | 328.75 | 122.56 |
| 1,000 | 2.0 | global_sort | yes | 0.03 | 0.37 | 336.45 | 328.75 | 122.56 |
| 10,000 | 0.5 | none | no | 0.00 | 0.79 | 1593.80 | 605.64 | 605.64 |
| 10,000 | 0.5 | none | yes | 0.00 | 0.64 | 768.84 | 605.64 | 605.64 |
| 10,000 | 0.5 | global_sort | no | 0.13 | 0.49 | 762.21 | 605.64 | 445.63 |
| 10,000 | 0.5 | global_sort | yes | 0.13 | 0.66 | 769.22 | 605.64 | 445.63 |
| 10,000 | 1.0 | none | no | 0.00 | 0.54 | 1103.56 | 503.34 | 503.34 |
| 10,000 | 1.0 | none | yes | 0.00 | 0.50 | 644.52 | 503.34 | 503.34 |
| 10,000 | 1.0 | global_sort | no | 0.12 | 0.46 | 603.18 | 503.34 | 325.23 |
| 10,000 | 1.0 | global_sort | yes | 0.11 | 0.55 | 625.25 | 503.34 | 325.23 |
| 10,000 | 1.5 | none | no | 0.00 | 0.51 | 680.99 | 394.81 | 394.81 |
| 10,000 | 1.5 | none | yes | 0.00 | 0.56 | 512.42 | 394.81 | 394.81 |
| 10,000 | 1.5 | global_sort | no | 0.08 | 0.33 | 366.38 | 394.81 | 195.58 |
| 10,000 | 1.5 | global_sort | yes | 0.08 | 0.48 | 457.61 | 394.81 | 195.58 |
| 10,000 | 2.0 | none | no | 0.00 | 0.44 | 490.89 | 333.90 | 333.90 |
| 10,000 | 2.0 | none | yes | 0.00 | 0.46 | 434.79 | 333.90 | 333.90 |
| 10,000 | 2.0 | global_sort | no | 0.05 | 0.23 | 231.80 | 333.90 | 128.71 |
| 10,000 | 2.0 | global_sort | yes | 0.05 | 0.38 | 351.95 | 333.90 | 128.71 |

## Observations

### Global sort vs entropy permutation

Global sort matches entropy permutation compression within 1-2 bits/key at high skew (s >= 1.5) while being 100-1000x faster. At M=10, Vocab=1K, s=2.0: global_sort=37.79 vs entropy=37.63 bits/key, but 0.01s vs 2.71s. The gap widens at low skew (s=0.5: 116.81 vs 115.50) but that's where permutation helps least. At M=10, entropy permutation ranges from 0.7s (s=2.0) to 229s (s=0.5, Vocab=10K). At M=50, entropy permutation did not complete within 20 min for s=1.5 — we only have M=50 results for global sort.

### When shared codebook helps

**Without permutation, shared codebook helps — sometimes dramatically.** When columns have similar distributions (no permutation), sharing one codebook avoids storing M redundant copies. The win is largest with large vocabulary and low skew. M=50, Vocab=10K, s=0.5: shared=769 vs independent=1594 bits/key (2x reduction). At M=10, Vocab=1K, s=0.5: shared=113 vs independent=142. The benefit shrinks as skew increases because the codebooks get smaller anyway.

**With permutation, shared codebook usually hurts.** Permutation makes columns have very different distributions — column 0 may be 99% one value while the last column is diverse. A shared codebook forces a compromise that can't give each column optimal code lengths. M=50, Vocab=1K, s=2.0 with global_sort: independent=210 vs shared=336 bits/key. The effect is consistent: at high skew with permutation, shared codebook adds 20-50% overhead.

### Permutation vs shared codebook

Both address the same underlying problem (per-column codebook overhead) from different angles. At low skew or large vocabulary, shared codebook alone can match or beat permutation — e.g. M=10, Vocab=10K, s=0.5: shared_cb alone=178.70 vs global_sort alone=210.81. But at moderate-to-high skew, permutation dominates — M=50, Vocab=1K, s=1.5: global_sort alone=271.92 vs shared_cb alone=451.09.

Combining them rarely helps. With permutation applied, adding shared codebook almost always makes things worse (the distributions are now too different to share). Without permutation, shared codebook is already capturing most of the available savings. The best strategy is: use global_sort + independent codebook when skew is moderate or higher (s >= 1.0); use no permutation + shared codebook when skew is very mild and vocabulary is large.

### Effect of M, N, and vocab size

**Larger M amplifies permutation benefit.** More columns means more opportunities to concentrate values into dominant positions. At s=1.0, Vocab=1K: M=10 gets 21% entropy reduction (78.56 to 60.09), M=50 gets 45% (429.14 to 235.30). The compression improvement from global_sort grows from ~20-40% at M=10 to ~40-55% at M=50. However, larger M also makes entropy permutation impractical (scales as O(N * M * |S|) per round).

**Larger vocabulary increases codebook overhead.** At M=10, s=2.0: Vocab=1K gets 37.79 bits/key, Vocab=10K gets 39.50 — similar because high skew means small effective codebooks. But at s=0.5: Vocab=1K gets 116.81 vs Vocab=10K gets 210.81 — the larger vocabulary bloats per-column codebook storage. This is also where shared codebook helps most (210.81 down to 178.76 with shared CB).

**N=10,000 is our only data point**, so we can't directly measure N scaling. Permutation time should scale linearly with N (both algorithms are O(N) per pass). CSF construction time also scales linearly. bits/key should be roughly N-independent for large enough N since both entropy and codebook overhead amortize.
