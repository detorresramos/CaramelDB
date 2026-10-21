# Benchmarks: main → staged-construction → fast-solve

Three commits, measured with `scripts/benchmark_staged.py` through the Python `Caramel()` API, on the same machine (Apple M-series, 14 threads):

- **main**: `ea7de68` (`results/final_baseline.json`)
- **staged**: `staged-construction`, `77819a0` (`results/final_new.json`)
- **fast**: `fast-solve`, `ac62826` (`results/fast_solve.json`)

Each case runs 3 times in fresh processes; the tables show medians. Every case returns correct values on all three commits. `main` and `staged` ran 85 cases and `fast` ran 79 of them. The main → staged column covers all 85 cases; everything else covers the 79.

## Summary

Ratios are new ÷ old: below 1 is better for every metric. Each cell is the median, with the range in parentheses.

| metric | main → staged | staged → fast | main → fast |
|---|---|---|---|
| Build time | 0.468 (0.015–0.970) | 0.090 (0.028–0.367) | 0.034 (0.002–0.226) |
| Peak build RAM | 1.000 (0.429–3.228) | 0.688 (0.109–1.070) | 0.589 (0.124–3.283) |
| Bits/key on disk | 1.000 (0.987–2.968) | 0.963 (0.914–0.987) | 0.963 (0.912–2.714) |
| Query time (Python loop) | 0.849 (0.076–2.287) | 0.855 (0.348–1.531) | 0.691 (0.066–1.747) |

Build speedup from main to fast: **29x** at the median (4.4x–486x).

## Caveats

- **Query time** is timed by a per-key Python loop. The Python call cost dominates it, and runs vary by up to 1.5x. Treat it as a rough signal. A C++ dependent-chain benchmark (staged → fast only, 10M keys) shows single-query latency at parity, except columns of near-unique values, which are ~20 ns slower (67 → 88 ns). No C++ latency numbers exist for main.
- **Peak RAM at 10M** is mostly the Python inputs (10M `str` objects and the value array), so it moves less than the build itself.

## Regressions against main

Two problems, both introduced by `staged-construction` and still present in `fast-solve`, not yet diagnosed: shared-filter multisets are ~2.7x larger on disk, and multisets with `filter=auto` use more peak RAM.

| case | metric | main | staged | fast |
|---|---|---:|---:|---:|
| multiset 1M×16 skew90 filter=auto shared filter | bits/key | 13.12 | 38.06 | 34.84 |
| multiset 1M×16 skew90 filter=auto shared codebook shared filter | bits/key | 12.66 | 37.58 | 34.36 |
| multiset 100k×32 uniform16 filter=auto | peak MB | 40 | 70 | 63 |
| multiset 1M×32 uniform16 filter=auto | peak MB | 193 | 622 | 632 |
| multiset 1M×32 zipf filter=auto | peak MB | 384 | 518 | 489 |
| multiset 4M×8 uniform16 filter=auto | peak MB | 505 | 670 | 621 |

## All cases

| case | build s<br>main | staged | fast | speedup<br>vs main | peak MB<br>main | staged | fast | bits/key<br>main | staged | fast | query ns<br>main | staged | fast |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| csf 100k uniform16 filter=none | 0.051 | 0.031 | 0.004 | 11x | 34 | 41 | 6 | 4.56 | 4.56 | 4.31 | 36 | 31 | 27 |
| csf 100k uniform16 filter=auto | 0.046 | 0.030 | 0.005 | 9x | 35 | 42 | 6 | 4.56 | 4.56 | 4.31 | 34 | 30 | 27 |
| csf 100k uniform64k filter=none | 0.355 | 0.085 | 0.014 | 26x | 36 | 49 | 13 | 34.12 | 34.09 | 33.45 | 144 | 64 | 71 |
| csf 100k uniform64k filter=auto | 0.332 | 0.085 | 0.015 | 23x | 36 | 50 | 12 | 34.12 | 34.09 | 33.45 | 114 | 64 | 72 |
| csf 100k zipf filter=none | 0.080 | 0.040 | 0.007 | 12x | 38 | 42 | 5 | 10.69 | 10.68 | 10.35 | 108 | 64 | 66 |
| csf 100k zipf filter=auto | 0.086 | 0.042 | 0.007 | 12x | 33 | 42 | 6 | 10.69 | 10.68 | 10.35 | 106 | 64 | 66 |
| csf 100k unique filter=none | 0.266 | 0.102 | 0.018 | 14x | 48 | 55 | 13 | 51.06 | 51.02 | 50.34 | 99 | 62 | 70 |
| csf 100k unique filter=auto | 0.264 | 0.102 | 0.019 | 14x | 42 | 50 | 13 | 51.06 | 51.02 | 50.34 | 100 | 62 | 70 |
| csf 100k skew90 filter=none | 0.025 | 0.012 | 0.004 | 6x | 25 | 29 | 5 | 2.67 | 2.67 | 2.47 | 57 | 38 | 24 |
| csf 100k skew90 filter=auto | 0.021 | 0.013 | 0.005 | 4x | 30 | 40 | 4 | 2.08 | 2.08 | 2.00 | 53 | 34 | 33 |
| csf 1M uniform16 filter=none | 0.723 | 0.235 | 0.029 | 25x | 120 | 93 | 56 | 4.55 | 4.54 | 4.30 | 68 | 58 | 43 |
| csf 1M uniform16 filter=auto | 0.732 | 0.237 | 0.031 | 24x | 120 | 98 | 56 | 4.55 | 4.54 | 4.30 | 57 | 64 | 43 |
| csf 1M uniform64k filter=none | 2.381 | 1.039 | 0.067 | 36x | 133 | 110 | 77 | 20.35 | 20.31 | 19.65 | 124 | 109 | 100 |
| csf 1M uniform64k filter=auto | 2.303 | 1.147 | 0.067 | 34x | 131 | 108 | 81 | 20.35 | 20.31 | 19.65 | 117 | 102 | 101 |
| csf 1M zipf filter=none | 1.063 | 0.390 | 0.042 | 25x | 129 | 97 | 59 | 9.70 | 9.69 | 9.35 | 129 | 145 | 114 |
| csf 1M zipf filter=auto | 1.082 | 0.421 | 0.045 | 24x | 126 | 97 | 60 | 9.70 | 9.69 | 9.35 | 128 | 118 | 124 |
| csf 1M unique filter=none | 3.148 | 1.545 | 0.191 | 16x | 198 | 179 | 152 | 54.81 | 54.76 | 53.87 | 124 | 235 | 119 |
| csf 1M unique filter=auto | 3.171 | 1.535 | 0.191 | 17x | 215 | 175 | 142 | 54.81 | 54.76 | 53.87 | 115 | 264 | 119 |
| csf 1M skew90 filter=none | 0.152 | 0.091 | 0.024 | 6x | 118 | 88 | 49 | 2.39 | 2.38 | 2.18 | 49 | 52 | 41 |
| csf 1M skew90 filter=auto | 0.176 | 0.107 | 0.028 | 6x | 105 | 94 | 49 | 1.76 | 1.76 | 1.69 | 59 | 84 | 45 |
| csf 10M uniform16 filter=none | 8.134 | 3.810 | 0.299 | 27x | 851 | 591 | 551 | 4.55 | 4.54 | 4.30 | 96 | 84 | 65 |
| csf 10M uniform16 filter=auto | 4.886 | 3.652 | 0.323 | 15x | 881 | 551 | 551 | 4.55 | 4.54 | 4.30 | 146 | 67 | 67 |
| csf 10M uniform64k filter=none | 16.517 | 13.376 | 0.685 | 24x | 921 | 614 | 551 | 18.47 | 18.44 | 17.76 | 268 | 180 | 248 |
| csf 10M uniform64k filter=auto | 16.020 | 13.312 | 0.613 | 26x | 926 | 496 | 489 | 18.47 | 18.44 | 17.76 | 235 | 180 | 149 |
| csf 10M zipf filter=none | 7.756 | 5.396 | 0.476 | 16x | 919 | 596 | 560 | 9.09 | 9.07 | 8.73 | 178 | 168 | 190 |
| csf 10M zipf filter=auto | 7.978 | 6.274 | 0.512 | 16x | 900 | 560 | 494 | 9.09 | 9.07 | 8.73 | 207 | 288 | 185 |
| csf 10M unique filter=none | 33.342 | 23.901 | 2.755 | 12x | 1809 | 1416 | 984 | 58.68 | 58.62 | 57.61 | 225 | 394 | 393 |
| csf 10M unique filter=auto | 38.088 | 23.351 | 2.836 | 13x | 1766 | 1332 | 908 | 58.68 | 58.62 | 57.61 | 225 | 393 | 389 |
| csf 10M skew90 filter=none | 2.136 | 0.904 | 0.247 | 9x | 870 | 586 | 560 | 2.36 | 2.35 | 2.15 | 69 | 128 | 44 |
| csf 10M skew90 filter=auto | 4.886 | 1.007 | 0.275 | 18x | 599 | 515 | 551 | 1.71 | 1.70 | 1.64 | 230 | 129 | 64 |
| multiset 100k×8 uniform16 filter=none | 1.089 | 0.180 | 0.014 | 77x | 36 | 42 | 5 | 36.44 | 36.44 | 34.44 | 844 | 430 | 377 |
| multiset 100k×8 uniform16 filter=auto | 1.394 | 0.177 | 0.015 | 91x | 35 | 45 | 16 | 36.44 | 36.44 | 34.44 | 965 | 427 | 375 |
| multiset 100k×8 zipf filter=none | 2.261 | 0.259 | 0.021 | 107x | 32 | 47 | 13 | 85.75 | 85.75 | 83.13 | 1868 | 756 | 680 |
| multiset 100k×8 zipf filter=auto | 6.963 | 0.263 | 0.024 | 294x | 37 | 50 | 20 | 85.75 | 85.75 | 83.13 | 4631 | 775 | 662 |
| multiset 100k×8 skew90 filter=none | 1.993 | 0.057 | 0.012 | 171x | 22 | 34 | 7 | 21.44 | 21.44 | 19.79 | 4511 | 527 | 405 |
| multiset 100k×8 skew90 filter=auto | 6.257 | 0.093 | 0.018 | 345x | 15 | 37 | 8 | 16.74 | 16.74 | 16.10 | 6982 | 570 | 490 |
| multiset 100k×8 unique filter=none | 24.609 | 0.950 | 0.052 | 469x | 85 | 101 | 69 | 408.18 | 408.18 | 402.70 | 4997 | 920 | 782 |
| multiset 100k×8 unique filter=auto | 24.558 | 0.833 | 0.057 | 433x | 98 | 110 | 70 | 408.18 | 408.18 | 402.70 | 11883 | 904 | 779 |
| multiset 100k×32 uniform16 filter=none | 14.482 | 0.911 | 0.036 | 408x | 39 | 45 | 8 | 145.73 | 145.73 | 137.74 | 4353 | 1026 | 845 |
| multiset 100k×32 uniform16 filter=auto | 19.253 | 0.787 | 0.040 | 486x | 40 | 70 | 63 | 145.73 | 145.73 | 137.74 | 9995 | 1027 | 830 |
| multiset 100k×32 zipf filter=none | 27.683 | 1.098 | 0.068 | 405x | 81 | 72 | 49 | 342.43 | 342.43 | 331.94 | 12439 | 2304 | 1889 |
| multiset 100k×32 zipf filter=auto | 3.580 | 1.056 | 0.074 | 48x | 73 | 98 | 64 | 342.43 | 342.43 | 331.94 | 2730 | 2244 | 1894 |
| multiset 100k×32 skew90 filter=none | 0.275 | 0.193 | 0.033 | 8x | 39 | 37 | 29 | 85.43 | 85.43 | 78.89 | 1405 | 1351 | 869 |
| multiset 100k×32 skew90 filter=auto | 2.397 | 0.396 | 0.059 | 41x | 41 | 44 | 29 | 66.53 | 66.52 | 64.00 | 2050 | 1603 | 1394 |
| multiset 100k×32 unique filter=none | 7.414 | 5.286 | 0.215 | 34x | 286 | 324 | 188 | 1632.70 | 1632.70 | 1610.84 | 3625 | 3339 | 2927 |
| multiset 100k×32 unique filter=auto | 12.670 | 4.802 | 0.232 | 55x | 278 | 347 | 188 | 1632.70 | 1632.70 | 1610.84 | 3705 | 3080 | 2689 |
| multiset 1M×8 uniform16 filter=none | 6.529 | 2.421 | 0.094 | 69x | 166 | 97 | 51 | 36.35 | 36.35 | 34.36 | 882 | 709 | 460 |
| multiset 1M×8 uniform16 filter=auto | 7.739 | 2.756 | 0.110 | 70x | 177 | 199 | 182 | 36.35 | 36.35 | 34.36 | 1135 | 600 | 460 |
| multiset 1M×8 zipf filter=none | 9.297 | 3.658 | 0.183 | 51x | 213 | 144 | 128 | 77.35 | 77.35 | 74.65 | 1254 | 985 | 824 |
| multiset 1M×8 zipf filter=auto | 6.288 | 3.997 | 0.201 | 31x | 200 | 169 | 144 | 77.35 | 77.35 | 74.65 | 1037 | 1003 | 865 |
| multiset 1M×8 skew90 filter=none | 0.777 | 0.456 | 0.076 | 10x | 150 | 91 | 72 | 19.03 | 19.03 | 17.42 | 675 | 655 | 464 |
| multiset 1M×8 skew90 filter=auto | 5.980 | 0.612 | 0.120 | 50x | 139 | 98 | 72 | 13.98 | 13.98 | 13.46 | 838 | 714 | 619 |
| multiset 1M×8 unique filter=none | 28.108 | 14.761 | 0.755 | 37x | 894 | 963 | 594 | 438.05 | 438.05 | 430.93 | 1992 | 2105 | 2160 |
| multiset 1M×8 unique filter=auto | 33.769 | 17.365 | 0.832 | 41x | 804 | 927 | 584 | 438.05 | 438.05 | 430.93 | 1776 | 1896 | 2057 |
| multiset 1M×32 uniform16 filter=none | 15.743 | 11.755 | 0.332 | 47x | 192 | 192 | 192 | 145.41 | 145.41 | 137.45 | 1335 | 1621 | 1080 |
| multiset 1M×32 uniform16 filter=auto | 14.125 | 11.893 | 0.499 | 28x | 193 | 622 | 632 | 145.41 | 145.41 | 137.45 | 1361 | 1316 | 1126 |
| multiset 1M×32 zipf filter=none | 25.476 | 19.073 | 0.771 | 33x | 384 | 384 | 384 | 309.56 | 309.56 | 298.77 | 3171 | 3411 | 2563 |
| multiset 1M×32 zipf filter=auto | 30.292 | 19.061 | 0.857 | 35x | 384 | 518 | 489 | 309.56 | 309.56 | 298.77 | 3692 | 3215 | 2538 |
| multiset 1M×32 skew90 filter=none | 5.771 | 3.123 | 0.271 | 21x | 288 | 288 | 288 | 76.08 | 76.08 | 69.66 | 2007 | 1499 | 1053 |
| multiset 1M×32 skew90 filter=auto | 26.907 | 2.997 | 0.452 | 60x | 288 | 288 | 288 | 55.91 | 55.92 | 53.84 | 2640 | 1894 | 1539 |
| multiset 1M×32 unique filter=none | 90.049 | 64.862 | 3.053 | 29x | 3185 | 3045 | 2023 | 1752.20 | 1752.20 | 1723.72 | 6947 | 7036 | 6933 |
| multiset 1M×32 unique filter=auto | 101.620 | 65.448 | 3.266 | 31x | 2877 | 2957 | 1964 | 1752.20 | 1752.20 | 1723.72 | 4757 | 6540 | 5133 |
| multiset 4M×8 uniform16 filter=none | 22.082 | 11.292 | 0.429 | 51x | 512 | 220 | 192 | 36.35 | 36.35 | 34.37 | 922 | 727 | 637 |
| multiset 4M×8 uniform16 filter=auto | 17.595 | 11.481 | 0.502 | 35x | 505 | 670 | 621 | 36.35 | 36.35 | 34.37 | 939 | 958 | 896 |
| multiset 4M×8 zipf filter=none | 28.334 | 19.443 | 0.839 | 34x | 598 | 384 | 384 | 74.21 | 74.21 | 71.47 | 1436 | 1303 | 1242 |
| multiset 4M×8 zipf filter=auto | 28.475 | 22.341 | 0.928 | 31x | 596 | 513 | 442 | 74.21 | 74.21 | 71.47 | 1388 | 1342 | 1369 |
| multiset 4M×8 skew90 filter=none | 4.307 | 3.017 | 0.320 | 13x | 484 | 288 | 288 | 18.83 | 18.83 | 17.22 | 892 | 975 | 539 |
| multiset 4M×8 skew90 filter=auto | 23.794 | 6.040 | 0.544 | 44x | 380 | 288 | 288 | 13.68 | 13.68 | 13.16 | 1076 | 963 | 671 |
| multiset 4M×8 unique filter=none | 137.884 | 81.666 | 3.676 | 38x | 3535 | 3790 | 2700 | 456.25 | 456.25 | 448.75 | 2545 | 1818 | 2783 |
| multiset 4M×8 unique filter=auto | 142.108 | 74.047 | 3.717 | 38x | 3588 | 4228 | 2700 | 456.25 | 456.25 | 448.75 | 2329 | 2223 | 1958 |
| multiset 1M×16 zipf filter=none shared codebook | 18.984 | 9.658 | 0.401 | 47x | 240 | 240 | 240 | 143.83 | 143.83 | 138.29 | 1869 | 1666 | 1335 |
| multiset 1M×16 skew90 filter=auto shared filter | 1.140 | 0.893 | 0.141 | 8x | 238 | 128 | 128 | 13.12 | 38.06 | 34.84 | 994 | 1059 | 669 |
| multiset 1M×16 skew90 filter=auto shared codebook shared filter | 1.172 | 0.927 | 0.138 | 9x | 241 | 128 | 128 | 12.66 | 37.58 | 34.36 | 1059 | 933 | 679 |
| ragged 100k×16 zipf filter=none | 3.893 | 0.421 | 0.069 | 56x | 51 | 52 | 30 | 98.25 | 98.24 | 95.20 | 1188 | 998 | 895 |
| ragged 100k×16 skew90 filter=auto | 0.876 | 0.198 | 0.061 | 14x | 36 | 43 | 20 | 24.42 | 24.40 | 23.45 | 869 | 687 | 613 |
| packed 100k×10 zipf | 0.536 | 0.223 | 0.048 | 11x | 57 | 59 | 18 | 61.02 | 60.35 | 56.37 | 732 | 665 | 602 |
| ragged 1M×16 zipf filter=none | 35.238 | 6.147 | 0.704 | 50x | 319 | 177 | 159 | 88.31 | 88.30 | 85.19 | 1721 | 1334 | 1014 |
| ragged 1M×16 skew90 filter=auto | 7.201 | 1.937 | 0.585 | 12x | 316 | 145 | 136 | 19.71 | 19.70 | 18.90 | 1096 | 1036 | 563 |
| packed 1M×10 zipf | 3.705 | 3.593 | 0.429 | 9x | 223 | 128 | 104 | 57.40 | 56.63 | 52.46 | 922 | 1061 | 583 |
