# CaramelDB

<p align="center">
  <img src="caramel.jpg" width="200" />
</p>

A high-performance C++/Python library for space-efficient static key-value stores. CaramelDB builds on the [compressed static function](https://vigna.di.unimi.it/ftp/papers/Compressed.pdf) (CSF), implementing it in C++ with improved construction and query performance, and extending it with prefilters, multiset support, and entropy-reducing permutations as described in [our paper](https://arxiv.org/pdf/2305.16545.pdf). The result is read-only key-value mappings stored in near-optimal space — often several times smaller than a hash table — with O(1) query time.

## Quick Start

### Installation

Clone the repository and all dependencies:

```bash
git clone --recurse-submodules https://github.com/detorresramos/CaramelDB.git
cd CaramelDB
```

We recommend using a virtual environment:

```bash
python3 -m venv .venv
source .venv/bin/activate
```

Install the required packages using our install scripts: `sh bin/install-linux.sh` or `sh bin/install-mac.sh`.
**Note:** The linux install script assumes apt as the package manager. You may need to modify the script if using a different package manager. Generally, we require g++, cmake, and OpenMP, in addition to pytest for the test suite.

Next, run our build script:

```bash
python3 bin/build.py
```

### Testing your installation

To run the Python tests, you can run

```bash
bin/python-test.sh
```

### Basic Usage

```python
from carameldb import Caramel, load

# Construct
csf = Caramel(
    keys=["red shoes", "black bags", "phone case"],
    values=[42, 17, 8],
)

# Query
csf.query("red shoes")  # 42

# Save and load
csf.save("map.csf")
csf = load("map.csf")
```

The backend is automatically inferred from the value types. Supported value types: `uint32`, `uint64`, fixed-length strings (10 or 12 chars), and variable-length strings. Keys can be `str` or `bytes`.

## Features

### Multiset Values

Pass 2D arrays to store multiple values per key. Use `permute=True` to rearrange columns for better compression.

```python
keys = ["query_a", "query_b", "query_c"]
values = [[1, 2, 3], [4, 5, 6], [1, 2, 7]]

csf = Caramel(keys, values, permute=True)
csf.query("query_a")  # [1, 2, 3]
```

### Prefilters

When one value dominates (e.g., 80%+ of keys map to the same value), a prefilter can significantly reduce size by handling the common value separately. Three filter types are supported:

```python
from carameldb import Caramel, BloomFilterConfig, XORFilterConfig, BinaryFuseFilterConfig

csf = Caramel(keys, values, prefilter=BloomFilterConfig(bits_per_element=10, num_hashes=7))
csf = Caramel(keys, values, prefilter=XORFilterConfig(fingerprint_bits=8))
csf = Caramel(keys, values, prefilter=BinaryFuseFilterConfig(fingerprint_bits=8))
```

### Stats

Inspect memory breakdown, Huffman coding statistics, and filter details:

```python
stats = csf.get_stats()
stats.in_memory_bytes       # total size
stats.solution_bytes        # CSF solution array
stats.filter_bytes          # prefilter (if any)
stats.huffman_stats          # code lengths, symbol count
stats.bucket_stats           # per-bucket solution sizes
```

## C++ API

The C++ API lives in `src/construct/Csf.h` (single-valued) and `src/construct/MultisetCsf.h` (multi-valued). The templated `Csf<T>` class supports custom value backends. See the [tests](tests/) for usage examples.

## Development

```bash
bin/build.py                  # build + install Python bindings
bin/build.py -t all           # build everything (including C++ tests)
bin/build.py -m Debug -t all  # debug build

bin/python-test.sh            # run Python tests
bin/cpp-test.sh               # run C++ tests

bin/python-format.sh          # format Python (black + isort)
bin/cpp-format.sh             # format C++
```

## Citation

If you use CaramelDB, please cite our paper:

```bibtex
@article{ramos2023caramel,
  title={CARAMEL: A Succinct Read-Only Lookup Table via Compressed Static Functions},
  author={Ramos, David Torres and Coleman, Benjamin and Lakshman, Vihan and Luo, Chen and Shrivastava, Anshumali},
  journal={arXiv preprint arXiv:2305.16545},
  year={2023}
}
```
