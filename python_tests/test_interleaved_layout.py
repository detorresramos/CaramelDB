"""Tests targeting the interleaved BucketArena layout and its build paths:
heterogeneous-distribution groups (subgrouping by bucket count), mixed filter
modes (multiple groups), the all-keys-filtered degenerate group, and large M
(stresses the flat arena indexing)."""

import carameldb
import numpy as np


def test_interleaved_heterogeneous_no_filter_group(tmp_path):
    """Columns with very different value distributions land in one no-filter
    group; subgrouping by target bucket count must still round-trip exactly."""
    num_rows = 2000
    rng = np.random.default_rng(7)
    cols = [
        np.full(num_rows, 5, dtype=np.uint32),               # constant
        rng.integers(0, 4, size=num_rows, dtype=np.uint32),   # low entropy
        rng.integers(0, num_rows, size=num_rows, dtype=np.uint32),  # high entropy
    ]
    values = np.stack(cols, axis=1)
    keys = [f"key_{i}" for i in range(num_rows)]

    csf = carameldb.Caramel(keys, values, verbose=False)
    for key, row in zip(keys, values):
        assert csf.query(key) == list(row)

    save_file = str(tmp_path / "hetero.csf")
    csf.save(save_file)
    csf2 = carameldb.load(save_file)
    for key, row in zip(keys, values):
        assert csf2.query(key) == list(row)


def test_interleaved_mixed_filter_modes_multiple_groups(tmp_path):
    """shared_filter with columns of different most-common values produces
    multiple filter groups; output_index routing must stay correct."""
    num_rows = 600
    rng = np.random.default_rng(11)
    values = np.zeros((num_rows, 4), dtype=np.uint32)
    # Column MCVs differ (0, 0, 7, 7) -> two shared-filter groups; a minority of
    # rows carry off-MCV values so the columns are non-degenerate.
    values[:, 2] = 7
    values[:, 3] = 7
    minority = rng.choice(num_rows, size=num_rows // 4, replace=False)
    for r in minority:
        values[r] = rng.integers(1, 20, size=4, dtype=np.uint32)

    prefilter = carameldb.BinaryFuseFilterConfig(fingerprint_bits=12)
    csf = carameldb.Caramel(
        keys=[f"key_{i}" for i in range(num_rows)],
        values=values,
        prefilter=prefilter,
        shared_filter=True,
        verbose=False,
    )
    keys = [f"key_{i}" for i in range(num_rows)]
    for key, row in zip(keys, values):
        assert csf.query(key) == list(row)

    save_file = str(tmp_path / "mixed.csf")
    csf.save(save_file)
    csf2 = carameldb.load(save_file)
    for key, row in zip(keys, values):
        assert csf2.query(key) == list(row)


def test_interleaved_degenerate_all_keys_filtered(tmp_path):
    """A group where every key matches the MCV produces a zero-bucket group.

    With a constant column, the filter removes all keys, so the group has no
    active keys / zero buckets. The degenerate path must still build, query,
    and round-trip.
    """
    num_rows = 400
    num_cols = 3
    keys = [f"key_{i}" for i in range(num_rows)]
    values = np.zeros((num_rows, num_cols), dtype=np.uint32)

    prefilter = carameldb.BinaryFuseFilterConfig(fingerprint_bits=12)
    csf = carameldb.Caramel(
        keys, values, prefilter=prefilter, shared_filter=True, verbose=False
    )
    for key, row in zip(keys, values):
        assert csf.query(key) == list(row)

    save_file = str(tmp_path / "degenerate.csf")
    csf.save(save_file)
    csf2 = carameldb.load(save_file)
    for key, row in zip(keys, values):
        assert csf2.query(key) == list(row)


def test_interleaved_large_m_correctness(tmp_path):
    """Large M (256 columns) in one no-filter group exercises the flat arena
    index arithmetic across many columns per bucket."""
    num_rows = 300
    num_cols = 256
    rng = np.random.default_rng(13)
    values = rng.integers(0, 50, size=(num_rows, num_cols), dtype=np.uint32)
    keys = [f"key_{i}" for i in range(num_rows)]

    csf = carameldb.Caramel(keys, values, verbose=False)
    for key, row in zip(keys, values):
        assert csf.query(key) == list(row)

    save_file = str(tmp_path / "largem.csf")
    csf.save(save_file)
    csf2 = carameldb.load(save_file)
    for key, row in zip(keys, values):
        assert csf2.query(key) == list(row)
