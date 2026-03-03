# Cython Migration Benchmark Results

Comparison of before (`121aa3f`) vs after (`c3586d8`). Changes include:

1. **Python bindings**: pybind11 → Cython (reduces per-call binding overhead)
2. **Huffman lookup table**: replaced sequential `canonicalDecodeFromNumber` with O(1) table lookup
3. **Query cache**: pre-computed `BucketQueryInfo` avoids per-query indirection through solution vectors
4. **Inlined bit extraction**: `always_inline` lambda on raw `uint64_t*` backing array, replacing `BitArray::getuint64()` bounds-checked method
5. **Eliminated string copies**: `const char*` + `size_t` overloads throughout query path (CSF, filters, hash), avoiding `std::string` heap allocation per call

All inference measured via Python `query()` loop (includes binding overhead).

- **Random**: different random keys each trial (cold cache, realistic workload)
- **Majority**: single majority-value key repeated (filter short-circuit path, hot cache)
- **Minority**: single minority-value key repeated (full CSF lookup path, hot cache)

Query measurement: 250 random keys/trial, 3 warmup + 10 measured trials, median ns/query.

## Comparison

| N | Alpha | Distribution | Construction Speedup | Random Speedup | Majority Speedup | Minority Speedup |
|--:|------:|:-------------|---------------------:|---------------:|-----------------:|-----------------:|
| 100,000 | 0.2 | zipfian | 1.59x | 2.69x | 3.79x | 4.64x |
| 100,000 | 0.2 | uniform_100 | 1.23x | 3.43x | 4.36x | 4.13x |
| 100,000 | 0.2 | unique | 1.08x | 2.55x | 4.57x | 4.09x |
| 100,000 | 0.5 | zipfian | 1.40x | 3.04x | 4.32x | 4.26x |
| 100,000 | 0.5 | uniform_100 | 1.40x | 3.43x | 4.44x | 4.30x |
| 100,000 | 0.5 | unique | 1.09x | 2.21x | 4.28x | 4.03x |
| 100,000 | 0.9 | zipfian | 2.25x | 3.49x | 4.49x | 4.23x |
| 100,000 | 0.9 | uniform_100 | 2.00x | 3.66x | 4.57x | 4.17x |
| 100,000 | 0.9 | unique | 1.94x | 3.93x | 4.36x | 4.54x |
| 10,000,000 | 0.2 | zipfian | 1.33x | 1.24x | 3.73x | 4.23x |
| 10,000,000 | 0.2 | uniform_100 | 1.12x | 1.93x | 4.33x | 4.32x |
| 10,000,000 | 0.2 | unique | 1.03x | 1.46x | 4.48x | 4.26x |
| 10,000,000 | 0.5 | zipfian | 1.38x | 1.50x | 4.27x | 4.03x |
| 10,000,000 | 0.5 | uniform_100 | 1.31x | 1.93x | 4.51x | 4.22x |
| 10,000,000 | 0.5 | unique | 1.10x | 1.16x | 4.32x | 4.33x |
| 10,000,000 | 0.9 | zipfian | 2.19x | 1.68x | 4.31x | 4.20x |
| 10,000,000 | 0.9 | uniform_100 | 2.04x | 1.80x | 4.28x | 4.27x |
| 10,000,000 | 0.9 | unique | 1.44x | 1.52x | 4.54x | 4.30x |
| 100,000,000 | 0.5 | zipfian | 1.29x | 1.33x | 4.35x | 4.10x |


## Before (pybind11)

| N | Alpha | Distribution | Construction (s) | Random (ns/q) | Majority (ns/q) | Minority (ns/q) | Size (bits/key) |
|--:|------:|:-------------|------------------:|--------------:|----------------:|----------------:|----------------:|
| 100,000 | 0.2 | zipfian | 0.05 | 210.8 | 162.5 | 163.9 | 11.153 |
| 100,000 | 0.2 | uniform_100 | 0.04 | 179.2 | 136.7 | 146.1 | 13.468 |
| 100,000 | 0.2 | unique | 0.10 | 232.5 | 145.1 | 162.7 | 47.872 |
| 100,000 | 0.5 | zipfian | 0.03 | 182.2 | 139.8 | 149.8 | 7.829 |
| 100,000 | 0.5 | uniform_100 | 0.04 | 189.7 | 145.4 | 150.1 | 8.474 |
| 100,000 | 0.5 | unique | 0.06 | 198.5 | 147.5 | 172.2 | 29.588 |
| 100,000 | 0.9 | zipfian | 0.02 | 146.7 | 152.5 | 158.0 | 1.760 |
| 100,000 | 0.9 | uniform_100 | 0.02 | 153.0 | 153.1 | 154.5 | 1.817 |
| 100,000 | 0.9 | unique | 0.03 | 167.1 | 151.7 | 191.2 | 5.751 |
| 10,000,000 | 0.2 | zipfian | 3.40 | 451.2 | 161.4 | 156.3 | 10.230 |
| 10,000,000 | 0.2 | uniform_100 | 4.54 | 478.7 | 144.9 | 153.9 | 13.019 |
| 10,000,000 | 0.2 | unique | 16.55 | 669.9 | 149.3 | 181.2 | 53.488 |
| 10,000,000 | 0.5 | zipfian | 2.80 | 462.7 | 147.9 | 150.5 | 7.127 |
| 10,000,000 | 0.5 | uniform_100 | 3.40 | 450.0 | 152.2 | 154.2 | 8.154 |
| 10,000,000 | 0.5 | unique | 10.22 | 563.3 | 149.1 | 183.5 | 33.091 |
| 10,000,000 | 0.9 | zipfian | 1.42 | 332.8 | 146.9 | 152.6 | 1.464 |
| 10,000,000 | 0.9 | uniform_100 | 1.54 | 326.2 | 147.9 | 152.4 | 1.652 |
| 10,000,000 | 0.9 | unique | 2.57 | 345.2 | 151.7 | 180.0 | 6.366 |
| 100,000,000 | 0.5 | zipfian | 34.64 | 521.8 | 147.0 | 149.0 | 7.066 |

## After (Cython + optimizations)

| N | Alpha | Distribution | Construction (s) | Random (ns/q) | Majority (ns/q) | Minority (ns/q) | Size (bits/key) |
|--:|------:|:-------------|------------------:|--------------:|----------------:|----------------:|----------------:|
| 100,000 | 0.2 | zipfian | 0.03 | 78.3 | 42.9 | 35.3 | 11.156 |
| 100,000 | 0.2 | uniform_100 | 0.04 | 52.2 | 31.3 | 35.3 | 13.465 |
| 100,000 | 0.2 | unique | 0.09 | 91.0 | 31.8 | 39.8 | 47.865 |
| 100,000 | 0.5 | zipfian | 0.02 | 59.9 | 32.4 | 35.1 | 7.826 |
| 100,000 | 0.5 | uniform_100 | 0.03 | 55.2 | 32.8 | 34.9 | 8.477 |
| 100,000 | 0.5 | unique | 0.06 | 89.8 | 34.4 | 42.7 | 29.586 |
| 100,000 | 0.9 | zipfian | 0.01 | 42.1 | 34.0 | 37.3 | 1.759 |
| 100,000 | 0.9 | uniform_100 | 0.01 | 41.8 | 33.5 | 37.0 | 1.818 |
| 100,000 | 0.9 | unique | 0.02 | 42.5 | 34.8 | 42.1 | 5.752 |
| 10,000,000 | 0.2 | zipfian | 2.55 | 363.1 | 43.3 | 36.9 | 10.231 |
| 10,000,000 | 0.2 | uniform_100 | 4.05 | 248.0 | 33.5 | 35.6 | 13.019 |
| 10,000,000 | 0.2 | unique | 16.03 | 459.2 | 33.4 | 42.5 | 53.488 |
| 10,000,000 | 0.5 | zipfian | 2.03 | 308.7 | 34.6 | 37.4 | 7.126 |
| 10,000,000 | 0.5 | uniform_100 | 2.60 | 233.2 | 33.7 | 36.5 | 8.154 |
| 10,000,000 | 0.5 | unique | 9.33 | 486.8 | 34.5 | 42.3 | 33.091 |
| 10,000,000 | 0.9 | zipfian | 0.65 | 197.6 | 34.1 | 36.3 | 1.464 |
| 10,000,000 | 0.9 | uniform_100 | 0.76 | 181.1 | 34.6 | 35.7 | 1.652 |
| 10,000,000 | 0.9 | unique | 1.78 | 227.0 | 33.4 | 41.9 | 6.366 |
| 100,000,000 | 0.5 | zipfian | 26.85 | 392.8 | 33.8 | 36.3 | 7.067 |
