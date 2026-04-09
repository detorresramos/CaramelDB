# Key-to-Array Benchmark Results

## Setup

- **N** (number of keys): 10,000
- **M** (values per key): exactly M for fixed-length arrays, or drawn uniformly from a range for ragged arrays
- **Vocab**: number of distinct values in the universe
- **s** (Zipfian exponent): controls value skew (0.5 = mild, 2.0 = high)
- Values sampled without replacement (per row) from Zipfian distribution over Vocab items

## Strategies

All strategies use AutoFilter prefilter. Permute variants use global_sort with 5 refinement iterations.

- **Column**: M independent CSFs, one per column position. Each column maps the keys to that keys value within the column.
- **Column+Permute**: Column, but first reorder values within each row to minimize per-column entropy.
- **Column+SharedCB**: Column with a single Huffman codebook shared across all columns instead of one per column.
- **Column+SharedCB+Permute**: Both shared codebook and permutation.
- **Packed**: Single CSF per key storing all values as a concatenated Huffman bitstream with a stop symbol. No column structure.
- **RaggedColumn**: A length CSF (key → array length) plus per-column CSFs. Column 0 has all keys, column 1 only keys with length >= 2, column i only keys with length >= i+1. Later columns shrink as fewer keys reach them.
- **RaggedColumn+Permute**: RaggedColumn with ragged-aware permutation applied across all columns (each row is permuted over its actual length, not just the shared prefix).
- **PaddedColumn**: Pad ragged arrays to max length, then use Column.
- **PaddedColumn+Permute**: Pad ragged arrays to max length, then use Column+Permute.

## Metrics

- **Bits per key**: serialized size in bits divided by N.
- **Construction time**: construction (includes permutation).
- **Query latency**: median nanoseconds per single-key lookup. 

## Results

### Fixed-Length Arrays


#### Bits per key (serialized size)

| M | Vocab | s | Column | Column+Permute | Column+SharedCB | Column+SharedCB+Permute | Packed |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 10 | 100 | 0.5 | 77.47 | **58.04** | 74.31 | 74.21 | 81.43 |
| 10 | 100 | 1.0 | 70.29 | **47.75** | 67.38 | 62.95 | 74.3 |
| 10 | 100 | 2.0 | 54.03 | **29.14** | 55.14 | 44.26 | 61.78 |
| 10 | 1000 | 0.5 | 142.07 | 116.76 | **113.41** | 113.49 | 122.22 |
| 10 | 1000 | 1.0 | 120.26 | 95.07 | 94.27 | **92.89** | 102.56 |
| 10 | 1000 | 2.0 | 62.73 | **37.77** | 60.41 | 50.17 | 67.24 |
| 25 | 100 | 0.5 | 195.11 | **110.18** | 186.57 | 184.6 | 202.3 |
| 25 | 100 | 1.0 | 184.71 | **93.45** | 178.7 | 158.9 | 193.72 |
| 25 | 100 | 2.0 | 159.03 | **68.73** | 165.12 | 130.34 | 179.2 |
| 25 | 1000 | 0.5 | 355.65 | **250.3** | 279.25 | 279.46 | 305.36 |
| 25 | 1000 | 1.0 | 313.57 | **203.37** | 242.72 | 232.36 | 265.18 |
| 25 | 1000 | 2.0 | 197.89 | **104.94** | 183.26 | 153.58 | 199.18 |

#### Construction time (seconds)

| M | Vocab | s | Column | Column+Permute | Column+SharedCB | Column+SharedCB+Permute | Packed |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 10 | 100 | 0.5 | 0.0884 | 0.062 | 0.0817 | 0.0662 | **0.0363** |
| 10 | 100 | 1.0 | 0.0605 | 0.0525 | 0.0605 | 0.0757 | **0.0342** |
| 10 | 100 | 2.0 | 0.0513 | 0.036 | 0.0575 | 0.0562 | **0.0305** |
| 10 | 1000 | 0.5 | 0.0883 | 0.1062 | 0.0901 | 0.1023 | **0.0527** |
| 10 | 1000 | 1.0 | 0.0779 | 0.0711 | 0.0819 | 0.0964 | **0.048** |
| 10 | 1000 | 2.0 | 0.0546 | 0.0329 | 0.0546 | 0.0593 | **0.0324** |
| 25 | 100 | 0.5 | 0.2081 | 0.1306 | 0.2205 | 0.2085 | **0.0855** |
| 25 | 100 | 1.0 | 0.206 | 0.1259 | 0.2302 | 0.1882 | **0.1048** |
| 25 | 100 | 2.0 | 0.1708 | **0.081** | 0.1713 | 0.1847 | 0.0851 |
| 25 | 1000 | 0.5 | 0.2898 | 0.2544 | 0.3114 | 0.3248 | **0.1504** |
| 25 | 1000 | 1.0 | 0.2693 | 0.2209 | 0.2901 | 0.2648 | **0.1143** |
| 25 | 1000 | 2.0 | 0.1956 | 0.1141 | 0.1864 | 0.1743 | **0.094** |

#### Query latency (ns/query)

| M | Vocab | s | Column | Column+Permute | Column+SharedCB | Column+SharedCB+Permute | Packed |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 10 | 100 | 0.5 | 258.2 | 276.9 | 254.4 | **252.3** | 320.8 |
| 10 | 100 | 1.0 | 264.2 | 282.3 | 267.9 | **258.3** | 386.5 |
| 10 | 100 | 2.0 | 332.0 | **254.1** | 276.6 | 273.9 | 351.7 |
| 10 | 1000 | 0.5 | 389.2 | 336.9 | 354.3 | **334.6** | 464.9 |
| 10 | 1000 | 1.0 | 327.8 | **326.8** | 327.8 | 336.6 | 455.3 |
| 10 | 1000 | 2.0 | 311.6 | **263.7** | 306.4 | 300.6 | 361.3 |
| 25 | 100 | 0.5 | 558.2 | 563.7 | **493.3** | 504.2 | 608.2 |
| 25 | 100 | 1.0 | 516.3 | 575.3 | **494.4** | 559.3 | 658.1 |
| 25 | 100 | 2.0 | 559.7 | **506.1** | 537.4 | 573.2 | 588.8 |
| 25 | 1000 | 0.5 | 928.8 | 727.8 | 790.4 | **727.6** | 922.3 |
| 25 | 1000 | 1.0 | 706.3 | 676.3 | **664.2** | 675.3 | 824.3 |
| 25 | 1000 | 2.0 | 621.8 | **575.6** | 659.6 | 669.3 | 702.8 |

### Ragged Arrays


#### Bits per key (serialized size)

| Length Range | Vocab | s | RaggedColumn | RaggedColumn+Permute | PaddedColumn | PaddedColumn+Permute | Packed |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| [5, 15] | 100 | 0.5 | 83.19 | **64.02** | 87.62 | 68.26 | 81.85 |
| [5, 15] | 100 | 1.0 | 76.18 | **54.27** | 80.79 | 58.72 | 74.93 |
| [5, 15] | 100 | 2.0 | 60.22 | **36.85** | 65.87 | 43.34 | 63.13 |
| [5, 15] | 1000 | 0.5 | 158.27 | 129.13 | 163.27 | 133.03 | **123.35** |
| [5, 15] | 1000 | 1.0 | 133.33 | 106.18 | 137.65 | 109.45 | **103.5** |
| [5, 15] | 1000 | 2.0 | 70.5 | **46.84** | 76.07 | 53.03 | 69.0 |
| [20, 40] | 100 | 0.5 | 242.98 | **131.76** | 255.5 | 145.08 | 247.08 |
| [20, 40] | 100 | 1.0 | 232.04 | **114.54** | 245.41 | 129.39 | 239.05 |
| [20, 40] | 100 | 2.0 | 204.47 | **89.07** | 218.02 | 102.95 | 225.1 |
| [20, 40] | 1000 | 0.5 | 456.04 | **305.95** | 468.48 | 318.06 | 376.01 |
| [20, 40] | 1000 | 1.0 | 403.25 | **251.59** | 416.4 | 266.72 | 329.51 |
| [20, 40] | 1000 | 2.0 | 262.68 | **139.16** | 276.23 | 152.58 | 253.31 |

#### Construction time (seconds)

| Length Range | Vocab | s | RaggedColumn | RaggedColumn+Permute | PaddedColumn | PaddedColumn+Permute | Packed |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| [5, 15] | 100 | 0.5 | 0.1052 | 0.0876 | 0.1169 | 0.0934 | **0.0427** |
| [5, 15] | 100 | 1.0 | 0.0935 | 0.0778 | 0.084 | 0.0721 | **0.0431** |
| [5, 15] | 100 | 2.0 | 0.0757 | 0.0617 | 0.0721 | 0.0473 | **0.0345** |
| [5, 15] | 1000 | 0.5 | 0.1479 | 0.1199 | 0.1285 | 0.1312 | **0.0597** |
| [5, 15] | 1000 | 1.0 | 0.1041 | 0.1077 | 0.1111 | 0.1121 | **0.0505** |
| [5, 15] | 1000 | 2.0 | 0.079 | 0.0643 | 0.0708 | 0.0623 | **0.0352** |
| [20, 40] | 100 | 0.5 | 0.3082 | 0.187 | 0.3244 | 0.1914 | **0.1142** |
| [20, 40] | 100 | 1.0 | 0.2773 | 0.188 | 0.2638 | 0.1596 | **0.0984** |
| [20, 40] | 100 | 2.0 | 0.2257 | 0.1217 | 0.2271 | 0.1295 | **0.1042** |
| [20, 40] | 1000 | 0.5 | 0.3539 | 0.3614 | 0.3768 | 0.3166 | **0.152** |
| [20, 40] | 1000 | 1.0 | 0.3509 | 0.2922 | 0.3721 | 0.2896 | **0.1414** |
| [20, 40] | 1000 | 2.0 | 0.3085 | 0.2058 | 0.2945 | 0.1905 | **0.1134** |

#### Query latency (ns/query)

| Length Range | Vocab | s | RaggedColumn | RaggedColumn+Permute | PaddedColumn | PaddedColumn+Permute | Packed |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| [5, 15] | 100 | 0.5 | **262.1** | 280.3 | 388.6 | 369.9 | 349.7 |
| [5, 15] | 100 | 1.0 | 306.1 | **291.3** | 389.5 | 404.3 | 375.6 |
| [5, 15] | 100 | 2.0 | 296.9 | **274.7** | 418.3 | 377.8 | 349.6 |
| [5, 15] | 1000 | 0.5 | 404.5 | **368.1** | 505.4 | 501.1 | 488.3 |
| [5, 15] | 1000 | 1.0 | **360.8** | 366.9 | 464.8 | 455.6 | 452.6 |
| [5, 15] | 1000 | 2.0 | 293.7 | **277.2** | 415.3 | 413.7 | 386.1 |
| [20, 40] | 100 | 0.5 | **672.7** | 703.2 | 794.2 | 903.8 | 686.5 |
| [20, 40] | 100 | 1.0 | **674.0** | 712.7 | 878.5 | 917.4 | 793.3 |
| [20, 40] | 100 | 2.0 | 675.5 | **654.8** | 937.1 | 886.0 | 743.7 |
| [20, 40] | 1000 | 0.5 | 976.1 | **923.1** | 1254.8 | 1170.2 | 1085.5 |
| [20, 40] | 1000 | 1.0 | **868.1** | 900.8 | 1082.8 | 1054.6 | 987.3 |
| [20, 40] | 1000 | 2.0 | 753.8 | **660.1** | 1005.2 | 950.8 | 823.9 |
