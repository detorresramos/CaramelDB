import os
import tempfile

import numpy as np
import pytest

import carameldb
from carameldb import AutoFilterConfig, GlobalSortPermutationConfig


def test_ragged_basic():
    """RaggedMultisetCSF with variable-length rows."""
    rng = np.random.default_rng(42)
    num_keys = 500
    vocab_size = 50

    keys = [f"key_{i}" for i in range(num_keys)]
    values = []
    for _ in range(num_keys):
        length = rng.integers(1, 10)
        row = sorted(rng.choice(vocab_size, size=length, replace=False).tolist())
        values.append(row)

    csf = carameldb.CaramelRagged(keys, values, verbose=False)

    for i in range(num_keys):
        result = csf.query(keys[i])
        assert sorted(result) == sorted(values[i]), (
            f"Key {keys[i]}: expected {values[i]}, got {result}"
        )


def test_ragged_fixed_length():
    """RaggedMultisetCSF also works with fixed-length rows."""
    rng = np.random.default_rng(123)
    num_keys = 300
    num_cols = 5
    vocab_size = 30

    keys = [f"key_{i}" for i in range(num_keys)]
    values = [
        sorted(rng.choice(vocab_size, size=num_cols, replace=False).tolist())
        for _ in range(num_keys)
    ]

    csf = carameldb.CaramelRagged(keys, values, verbose=False)

    for i in range(num_keys):
        result = csf.query(keys[i])
        assert sorted(result) == sorted(values[i])


def test_ragged_with_permutation():
    """Permutation on the min_length prefix."""
    rng = np.random.default_rng(99)
    num_keys = 400
    vocab_size = 40

    keys = [f"key_{i}" for i in range(num_keys)]
    # All rows have at least 3 values, some have up to 8
    values = []
    for _ in range(num_keys):
        length = rng.integers(3, 9)
        row = sorted(rng.choice(vocab_size, size=length, replace=False).tolist())
        values.append(row)

    csf = carameldb.CaramelRagged(
        keys, values,
        permutation=GlobalSortPermutationConfig(refinement_iterations=3),
        verbose=False,
    )

    for i in range(num_keys):
        result = csf.query(keys[i])
        assert sorted(result) == sorted(values[i])


def test_ragged_shared_codebook():
    """Shared codebook across columns."""
    rng = np.random.default_rng(77)
    num_keys = 300
    vocab_size = 30

    keys = [f"key_{i}" for i in range(num_keys)]
    values = []
    for _ in range(num_keys):
        length = rng.integers(2, 7)
        row = sorted(rng.choice(vocab_size, size=length, replace=False).tolist())
        values.append(row)

    csf = carameldb.CaramelRagged(
        keys, values, shared_codebook=True, verbose=False,
    )

    for i in range(num_keys):
        result = csf.query(keys[i])
        assert sorted(result) == sorted(values[i])


def test_ragged_save_load():
    """Round-trip save/load."""
    keys = [f"key_{i}" for i in range(200)]
    values = [[i % 10, (i * 3) % 10] if i % 2 == 0 else [i % 10] for i in range(200)]

    csf = carameldb.CaramelRagged(keys, values, verbose=False)

    with tempfile.NamedTemporaryFile(suffix=".csf", delete=False) as tmp:
        tmp_path = tmp.name
    try:
        csf.save(tmp_path)
        loaded = carameldb.RaggedMultisetCSFUint32.load(tmp_path)
        for i in range(200):
            assert loaded.query(keys[i]) == csf.query(keys[i])
    finally:
        os.unlink(tmp_path)


def test_ragged_single_element_rows():
    """All rows have exactly 1 element."""
    keys = [f"k{i}" for i in range(100)]
    values = [[i % 20] for i in range(100)]

    csf = carameldb.CaramelRagged(keys, values, verbose=False)

    for i in range(100):
        assert csf.query(keys[i]) == values[i]
