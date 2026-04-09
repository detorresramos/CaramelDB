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
- **RaggedColumn+Permute**: RaggedColumn with permutation applied to the prefix columns shared by all keys.
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
| 10 | 100 | 0.5 | 0.0846 | 0.0598 | 0.0809 | 0.0693 | **0.0343** |
| 10 | 100 | 1.0 | 0.061 | 0.0509 | 0.0619 | 0.0653 | **0.0304** |
| 10 | 100 | 2.0 | 0.0489 | 0.0321 | 0.0552 | 0.0549 | **0.0293** |
| 10 | 1000 | 0.5 | 0.0874 | 0.1058 | 0.0908 | 0.1025 | **0.0515** |
| 10 | 1000 | 1.0 | 0.078 | 0.0709 | 0.084 | 0.1009 | **0.0483** |
| 10 | 1000 | 2.0 | 0.0571 | **0.034** | 0.0573 | 0.0616 | 0.0374 |
| 25 | 100 | 0.5 | 0.2141 | 0.1386 | 0.2289 | 0.2223 | **0.0906** |
| 25 | 100 | 1.0 | 0.2044 | 0.1248 | 0.2295 | 0.1912 | **0.1141** |
| 25 | 100 | 2.0 | 0.177 | **0.0822** | 0.2027 | 0.1694 | 0.1012 |
| 25 | 1000 | 0.5 | 0.3104 | 0.2786 | 0.3058 | 0.3062 | **0.1347** |
| 25 | 1000 | 1.0 | 0.279 | 0.2092 | 0.2681 | 0.2581 | **0.121** |
| 25 | 1000 | 2.0 | 0.2032 | 0.1499 | 0.1959 | 0.1906 | **0.1006** |

#### Query latency (ns/query)

| M | Vocab | s | Column | Column+Permute | Column+SharedCB | Column+SharedCB+Permute | Packed |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 10 | 100 | 0.5 | 44187.1 | 43943.2 | 43849.1 | 44355.1 | **299.8** |
| 10 | 100 | 1.0 | 44112.9 | 43982.2 | 44264.0 | 43798.4 | **339.3** |
| 10 | 100 | 2.0 | 44139.4 | 43799.4 | 43989.7 | 44038.7 | **341.0** |
| 10 | 1000 | 0.5 | 44442.2 | 44354.9 | 44379.4 | 44172.5 | **416.4** |
| 10 | 1000 | 1.0 | 44105.5 | 43657.4 | 44001.8 | 44185.9 | **406.2** |
| 10 | 1000 | 2.0 | 43882.9 | 43798.6 | 43813.7 | 44033.7 | **341.7** |
| 25 | 100 | 0.5 | 44036.2 | 43659.1 | 44029.3 | 44108.2 | **563.3** |
| 25 | 100 | 1.0 | 44117.1 | 44139.9 | 44020.4 | 44642.3 | **648.7** |
| 25 | 100 | 2.0 | 44438.3 | 44688.2 | 43968.0 | 44128.7 | **600.6** |
| 25 | 1000 | 0.5 | 43780.2 | 45360.2 | 44599.9 | 44238.6 | **859.7** |
| 25 | 1000 | 1.0 | 43165.8 | 44220.5 | 44230.8 | 44403.6 | **774.3** |
| 25 | 1000 | 2.0 | 44285.8 | 43927.5 | 43383.7 | 44405.2 | **653.3** |

### Ragged Arrays


#### Bits per key (serialized size)

| Length Range | Vocab | s | RaggedColumn | RaggedColumn+Permute | PaddedColumn | PaddedColumn+Permute | Packed |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| [5, 15] | 100 | 0.5 | 83.19 | 77.63 | 87.62 | **68.26** | 81.85 |
| [5, 15] | 100 | 1.0 | 76.18 | 69.55 | 80.79 | **58.72** | 74.93 |
| [5, 15] | 100 | 2.0 | 60.22 | 51.51 | 65.87 | **43.34** | 63.13 |
| [5, 15] | 1000 | 0.5 | 158.27 | 150.88 | 163.27 | 133.03 | **123.35** |
| [5, 15] | 1000 | 1.0 | 133.33 | 126.23 | 137.65 | 109.45 | **103.5** |
| [5, 15] | 1000 | 2.0 | 70.5 | 61.7 | 76.07 | **53.03** | 69.0 |
| [20, 40] | 100 | 0.5 | 242.98 | 182.76 | 255.5 | **145.08** | 247.08 |
| [20, 40] | 100 | 1.0 | 232.04 | 165.92 | 245.41 | **129.39** | 239.05 |
| [20, 40] | 100 | 2.0 | 204.47 | 137.62 | 218.02 | **102.95** | 225.1 |
| [20, 40] | 1000 | 0.5 | 456.04 | 380.49 | 468.48 | **318.06** | 376.01 |
| [20, 40] | 1000 | 1.0 | 403.25 | 324.9 | 416.4 | **266.72** | 329.51 |
| [20, 40] | 1000 | 2.0 | 262.68 | 195.8 | 276.23 | **152.58** | 253.31 |

#### Construction time (seconds)

| Length Range | Vocab | s | RaggedColumn | RaggedColumn+Permute | PaddedColumn | PaddedColumn+Permute | Packed |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| [5, 15] | 100 | 0.5 | 0.1111 | 0.1082 | 0.113 | 0.1005 | **0.0496** |
| [5, 15] | 100 | 1.0 | 0.0992 | 0.0963 | 0.0905 | 0.0792 | **0.0511** |
| [5, 15] | 100 | 2.0 | 0.0959 | 0.0769 | 0.0845 | 0.0544 | **0.0421** |
| [5, 15] | 1000 | 0.5 | 0.1558 | 0.1663 | 0.147 | 0.1408 | **0.0731** |
| [5, 15] | 1000 | 1.0 | 0.1227 | 0.1243 | 0.1157 | 0.1177 | **0.0791** |
| [5, 15] | 1000 | 2.0 | 0.0917 | 0.0819 | 0.0815 | 0.0717 | **0.0376** |
| [20, 40] | 100 | 0.5 | 0.3278 | 0.2603 | 0.3479 | 0.2055 | **0.1192** |
| [20, 40] | 100 | 1.0 | 0.3039 | 0.2292 | 0.316 | 0.1955 | **0.1354** |
| [20, 40] | 100 | 2.0 | 0.3044 | 0.2036 | 0.2864 | 0.1512 | **0.1089** |
| [20, 40] | 1000 | 0.5 | 0.3833 | 0.3828 | 0.4431 | 0.3758 | **0.2014** |
| [20, 40] | 1000 | 1.0 | 0.4242 | 0.3196 | 0.3916 | 0.3185 | **0.1982** |
| [20, 40] | 1000 | 2.0 | 0.3097 | 0.2285 | 0.308 | 0.1686 | **0.122** |

#### Query latency (ns/query)

| Length Range | Vocab | s | RaggedColumn | RaggedColumn+Permute | PaddedColumn | PaddedColumn+Permute | Packed |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| [5, 15] | 100 | 0.5 | **258.6** | 271.0 | 43833.3 | 43987.7 | 358.0 |
| [5, 15] | 100 | 1.0 | **267.4** | 291.0 | 43997.6 | 44196.7 | 344.6 |
| [5, 15] | 100 | 2.0 | 282.7 | **260.6** | 44170.8 | 44056.1 | 332.8 |
| [5, 15] | 1000 | 0.5 | 379.5 | **367.6** | 44279.1 | 44012.5 | 432.5 |
| [5, 15] | 1000 | 1.0 | **337.6** | 410.8 | 44389.0 | 44368.3 | 418.2 |
| [5, 15] | 1000 | 2.0 | 282.8 | **278.5** | 43981.3 | 44223.9 | 345.7 |
| [20, 40] | 100 | 0.5 | **581.8** | 616.9 | 44548.2 | 44735.2 | 675.6 |
| [20, 40] | 100 | 1.0 | **621.5** | 624.0 | 44418.5 | 44261.2 | 753.2 |
| [20, 40] | 100 | 2.0 | 626.5 | **599.7** | 44204.6 | 44211.4 | 690.2 |
| [20, 40] | 1000 | 0.5 | 934.3 | **855.1** | 44981.1 | 44971.0 | 1061.7 |
| [20, 40] | 1000 | 1.0 | **829.1** | 854.3 | 44469.2 | 44302.8 | 1056.7 |
| [20, 40] | 1000 | 2.0 | 720.9 | **698.3** | 44373.8 | 43927.7 | 815.5 |
