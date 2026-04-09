# Key-to-Array Benchmark Results

## Setup

- **N** (number of keys): 10,000
- **M** (values per key): exactly M for fixed-length arrays, or drawn uniformly from a range for ragged arrays
- **Vocab**: number of distinct values in the universe
- **s** (Zipfian exponent): controls value skew (0.5 = mild, 2.0 = high)
- Values sampled without replacement (per row) from Zipfian distribution over Vocab items

All strategies use AutoFilter prefilter. Permute variants use global_sort with 5 refinement iterations.

## Fixed Strategies

- **Column**: M independent CSFs, one per column position. Each column maps the keys to that keys value within the column.
- **Column+Permute**: Column, but first reorder values within each row to minimize per-column entropy.
- **Column+SharedCB**: Column with a single Huffman codebook shared across all columns instead of one per column.
- **Column+SharedCB+Permute**: Both shared codebook and permutation.
- **Packed**: Single CSF per key storing all values as a concatenated Huffman bitstream with a stop symbol. No column structure.

## Ragged Strategies

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

| Length Range | Vocab | s | RaggedColumn | RaggedColumn+Permute | RaggedColumn+SharedCB | RaggedColumn+SharedCB+Permute | PaddedColumn | PaddedColumn+Permute | Packed |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| [5, 15] | 100 | 0.5 | 83.19 | **64.02** | 78.22 | 78.18 | 87.62 | 68.26 | 81.85 |
| [5, 15] | 100 | 1.0 | 76.18 | **54.27** | 71.62 | 67.93 | 80.79 | 58.72 | 74.93 |
| [5, 15] | 100 | 2.0 | 60.22 | **36.85** | 60.24 | 50.21 | 65.87 | 43.34 | 63.13 |
| [5, 15] | 1000 | 0.5 | 158.27 | 129.13 | **117.24** | 117.27 | 163.27 | 133.03 | 123.35 |
| [5, 15] | 1000 | 1.0 | 133.33 | 106.18 | 98.35 | **97.14** | 137.65 | 109.45 | 103.5 |
| [5, 15] | 1000 | 2.0 | 70.5 | **46.84** | 65.59 | 57.76 | 76.07 | 53.03 | 69.0 |
| [20, 40] | 100 | 0.5 | 242.98 | **131.76** | 229.22 | 224.28 | 255.5 | 145.08 | 247.08 |
| [20, 40] | 100 | 1.0 | 232.04 | **114.54** | 221.83 | 195.85 | 245.41 | 129.39 | 239.05 |
| [20, 40] | 100 | 2.0 | 204.47 | **89.07** | 208.99 | 164.68 | 218.02 | 102.95 | 225.1 |
| [20, 40] | 1000 | 0.5 | 456.04 | **305.95** | 339.06 | 339.08 | 468.48 | 318.06 | 376.01 |
| [20, 40] | 1000 | 1.0 | 403.25 | **251.59** | 298.86 | 284.65 | 416.4 | 266.72 | 329.51 |
| [20, 40] | 1000 | 2.0 | 262.68 | **139.16** | 233.16 | 190.71 | 276.23 | 152.58 | 253.31 |

#### Construction time (seconds)

| Length Range | Vocab | s | RaggedColumn | RaggedColumn+Permute | RaggedColumn+SharedCB | RaggedColumn+SharedCB+Permute | PaddedColumn | PaddedColumn+Permute | Packed |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| [5, 15] | 100 | 0.5 | 0.1225 | 0.1011 | 0.1101 | 0.1164 | 0.1605 | 0.1039 | **0.0512** |
| [5, 15] | 100 | 1.0 | 0.1215 | 0.0958 | 0.1115 | 0.1125 | 0.1023 | 0.0932 | **0.0567** |
| [5, 15] | 100 | 2.0 | 0.0805 | 0.0671 | 0.0885 | 0.0929 | 0.0819 | 0.0528 | **0.0386** |
| [5, 15] | 1000 | 0.5 | 0.1506 | 0.1301 | 0.1383 | 0.1428 | 0.1339 | 0.1488 | **0.0669** |
| [5, 15] | 1000 | 1.0 | 0.1154 | 0.1171 | 0.1271 | 0.1344 | 0.1255 | 0.116 | **0.0534** |
| [5, 15] | 1000 | 2.0 | 0.0873 | 0.069 | 0.1032 | 0.1012 | 0.0833 | 0.0636 | **0.0424** |
| [20, 40] | 100 | 0.5 | 0.3266 | 0.2141 | 0.3213 | 0.31 | 0.3333 | 0.2079 | **0.1115** |
| [20, 40] | 100 | 1.0 | 0.2769 | 0.1945 | 0.3118 | 0.3037 | 0.2964 | 0.1891 | **0.1081** |
| [20, 40] | 100 | 2.0 | 0.2398 | 0.1297 | 0.2811 | 0.2403 | 0.2746 | 0.1561 | **0.1117** |
| [20, 40] | 1000 | 0.5 | 0.3866 | 0.3752 | 0.4095 | 0.4425 | 0.4221 | 0.3515 | **0.1815** |
| [20, 40] | 1000 | 1.0 | 0.3515 | 0.297 | 0.4056 | 0.3841 | 0.3996 | 0.2947 | **0.1418** |
| [20, 40] | 1000 | 2.0 | 0.2805 | 0.1952 | 0.2787 | 0.2554 | 0.2876 | 0.1654 | **0.1302** |

#### Query latency (ns/query)

| Length Range | Vocab | s | RaggedColumn | RaggedColumn+Permute | RaggedColumn+SharedCB | RaggedColumn+SharedCB+Permute | PaddedColumn | PaddedColumn+Permute | Packed |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| [5, 15] | 100 | 0.5 | 266.2 | 291.6 | **265.7** | 294.1 | 351.9 | 361.1 | 337.3 |
| [5, 15] | 100 | 1.0 | 270.7 | 289.2 | **261.2** | 285.6 | 353.5 | 362.9 | 333.8 |
| [5, 15] | 100 | 2.0 | **271.7** | 278.5 | 305.1 | 277.6 | 373.7 | 352.8 | 358.2 |
| [5, 15] | 1000 | 0.5 | 378.1 | 366.9 | 380.3 | **364.6** | 463.3 | 439.8 | 472.9 |
| [5, 15] | 1000 | 1.0 | 330.5 | 331.8 | **321.0** | 337.3 | 409.2 | 410.4 | 491.6 |
| [5, 15] | 1000 | 2.0 | 287.5 | **272.3** | 347.6 | 314.9 | 385.4 | 443.1 | 379.9 |
| [20, 40] | 100 | 0.5 | 605.0 | 668.5 | **564.8** | 567.6 | 803.2 | 951.8 | 674.9 |
| [20, 40] | 100 | 1.0 | 612.3 | 649.7 | **595.4** | 631.0 | 909.9 | 901.0 | 784.8 |
| [20, 40] | 100 | 2.0 | 623.5 | **601.8** | 637.8 | 634.2 | 947.1 | 799.8 | 711.9 |
| [20, 40] | 1000 | 0.5 | 1035.6 | **843.3** | 924.7 | 865.4 | 1192.3 | 1073.6 | 1082.5 |
| [20, 40] | 1000 | 1.0 | 834.7 | 803.8 | **788.2** | 848.3 | 991.9 | 985.2 | 1022.4 |
| [20, 40] | 1000 | 2.0 | 683.4 | **680.2** | 821.3 | 741.2 | 909.6 | 938.2 | 812.8 |
