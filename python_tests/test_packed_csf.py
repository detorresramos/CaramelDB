import os
import tempfile

import numpy as np
import pytest

import carameldb


def test_packed_csf_basic_fixed():
    """PackedCSF with fixed-length rows."""
    rng = np.random.default_rng(42)
    num_keys = 500
    num_cols = 5
    vocab_size = 50

    keys = [f"key_{i}" for i in range(num_keys)]
    values = [
        sorted(rng.choice(vocab_size, size=num_cols, replace=False).tolist())
        for _ in range(num_keys)
    ]

    csf = carameldb.CaramelPacked(keys, values, verbose=False)

    for i in range(num_keys):
        result = csf.query(keys[i])
        assert sorted(result) == sorted(values[i]), (
            f"Key {keys[i]}: expected {values[i]}, got {result}"
        )


def test_packed_csf_ragged():
    """PackedCSF with variable-length (ragged) rows."""
    rng = np.random.default_rng(123)
    num_keys = 300
    vocab_size = 30

    keys = [f"key_{i}" for i in range(num_keys)]
    values = []
    for _ in range(num_keys):
        length = rng.integers(1, 10)
        row = sorted(rng.choice(vocab_size, size=length, replace=False).tolist())
        values.append(row)

    csf = carameldb.CaramelPacked(keys, values, verbose=False)

    for i in range(num_keys):
        result = csf.query(keys[i])
        assert sorted(result) == sorted(values[i]), (
            f"Key {keys[i]}: expected {values[i]}, got {result}"
        )


def test_packed_csf_single_value_rows():
    """Each key maps to a single value."""
    keys = [f"k{i}" for i in range(100)]
    values = [[i % 20] for i in range(100)]

    csf = carameldb.CaramelPacked(keys, values, verbose=False)

    for i in range(100):
        assert csf.query(keys[i]) == values[i]


def test_packed_csf_save_load():
    """Round-trip save/load."""
    keys = [f"key_{i}" for i in range(200)]
    values = [[i % 10, (i * 3) % 10, (i * 7) % 10] for i in range(200)]

    csf = carameldb.CaramelPacked(keys, values, verbose=False)

    with tempfile.NamedTemporaryFile(suffix=".csf", delete=False) as tmp:
        tmp_path = tmp.name
    try:
        csf.save(tmp_path)
        loaded = carameldb.PackedCSFUint32.load(tmp_path)
        for i in range(200):
            assert loaded.query(keys[i]) == csf.query(keys[i])
    finally:
        os.unlink(tmp_path)


def test_packed_csf_numpy_input():
    """CaramelPacked accepts 2D numpy arrays."""
    keys = [f"key_{i}" for i in range(100)]
    values = np.array([[i % 5, (i + 1) % 5, (i + 2) % 5] for i in range(100)],
                      dtype=np.uint32)

    csf = carameldb.CaramelPacked(keys, values, verbose=False)

    for i in range(100):
        assert sorted(csf.query(keys[i])) == sorted(values[i].tolist())
