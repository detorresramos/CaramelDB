import random

import carameldb
import numpy as np
import pytest


def test_multiset_csf(tmp_path):
    num_rows = 1000
    num_columns = 10
    keys = [f"key_{i}" for i in range(num_rows)]
    values = [[j + i for j in range(num_columns)] for i in range(num_rows)]

    csf = carameldb.Caramel(keys, values)
    assert isinstance(csf, carameldb.MultisetCSFUint32)

    for key, value in zip(keys, values):
        assert csf.query(key) == value

    save_file = str(tmp_path / "multiset.csf")
    csf.save(save_file)
    csf = carameldb.load(save_file)

    for key, value in zip(keys, values):
        assert csf.query(key) == value


def test_multiset_csf_different_number_of_values():
    with pytest.raises(
        ValueError,
        match="Error transforming values to numpy array. Make sure all rows are the same length.",
    ):
        carameldb.Caramel(["1", "2", "3"], [[1, 2], [2, 3], [3]])


def test_multiset_csf_empty_lists():
    with pytest.raises(
        ValueError,
        match="Subarray must not be empty.",
    ):
        carameldb.Caramel(["1", "2", "3"], [[], [], []])


def test_multiset_csf_strings():
    csf = carameldb.Caramel(["1", "2", "3"], [["lol"], ["lol"], ["loll"]])
    assert isinstance(csf, carameldb.MultisetCSFString)


def test_multiset_csf_permute(tmp_path):
    num_rows = 1000
    num_columns = 10
    keys = [f"key_{i}" for i in range(num_rows)]
    values = []
    for _ in range(num_rows):
        lst = [j for j in range(num_columns)]
        random.shuffle(lst)
        values.append(lst)

    values = np.array(values)

    csf_permute = carameldb.Caramel(keys, values, permute=True, verbose=False)
    csf_permute_file = tmp_path / "csf_permute.csf"
    csf_permute.save(str(csf_permute_file))
    csf_permute_size = csf_permute_file.stat().st_size

    csf_no_permute = carameldb.Caramel(keys, values, permute=False, verbose=False)
    csf_no_permute_file = tmp_path / "csf_no_permute.csf"
    csf_no_permute.save(str(csf_no_permute_file))
    csf_no_permute_size = csf_no_permute_file.stat().st_size

    assert csf_permute_size < csf_no_permute_size


def test_permutation():
    values = np.array(
        [[0, 1, 2, 3, 4, 5, 6, 0], [1, 0, 0, 1, 0, 1, 2, 2]], dtype=np.uint32
    )
    carameldb.permute_uint32(values)

    assert values[0][0] == 2
    assert values[1][0] == 2

    values = np.array(
        [
            [str(x).ljust(10) for x in [0, 1, 2, 3, 4, 5, 6, 0]],
            [str(x).ljust(10) for x in [1, 0, 0, 1, 0, 1, 2, 2]],
        ],
        dtype="|S10",
    )
    carameldb.permute_char10(values)

    assert values[0][0] == str(2).ljust(10).encode()
    assert values[1][0] == str(2).ljust(10).encode()


def test_multiset_direct_constructor():
    num_rows = 100
    num_cols = 5
    keys = [f"key_{i}" for i in range(num_rows)]
    values = np.array(
        [[j + i for j in range(num_cols)] for i in range(num_rows)], dtype=np.uint32
    )

    csf = carameldb.MultisetCSFUint32(keys, values, verbose=False)
    for key, row in zip(keys, values):
        assert csf.query(key) == list(row)


def test_multiset_direct_constructor_permute():
    num_rows = 100
    num_cols = 5
    keys = [f"key_{i}" for i in range(num_rows)]
    values = np.array(
        [[j + i for j in range(num_cols)] for i in range(num_rows)], dtype=np.uint32
    )

    csf = carameldb.MultisetCSFUint32(keys, values, permute=True, verbose=False)
    for key, row in zip(keys, values):
        assert sorted(csf.query(key)) == sorted(row)


def test_multiset_string_permute_raises():
    keys = ["a", "b", "c"]
    values = np.array([["x", "y"], ["a", "b"], ["c", "d"]])
    with pytest.raises(
        ValueError, match="'permute' is not supported for variable-length string values."
    ):
        carameldb.MultisetCSFString(keys, values, permute=True)


def test_multiset_csf_shared_codebook(tmp_path):
    num_rows = 500
    num_cols = 5
    keys = [f"key_{i}" for i in range(num_rows)]
    values = np.random.randint(0, 10, size=(num_rows, num_cols), dtype=np.uint32)

    csf = carameldb.Caramel(keys, values, shared_codebook=True, verbose=False)
    for key, row in zip(keys, values):
        assert csf.query(key) == list(row)

    save_file = str(tmp_path / "shared_cb.csf")
    csf.save(save_file)
    csf2 = carameldb.load(save_file)
    for key, row in zip(keys, values):
        assert csf2.query(key) == list(row)


def test_multiset_csf_shared_filter(tmp_path):
    num_rows = 500
    num_cols = 5
    keys = [f"key_{i}" for i in range(num_rows)]
    # Most values are 0 so the filter has something to filter
    values = np.zeros((num_rows, num_cols), dtype=np.uint32)
    for i in range(num_rows // 5):
        values[i] = np.random.randint(1, 10, size=num_cols, dtype=np.uint32)

    prefilter = carameldb.BinaryFuseFilterConfig(fingerprint_bits=12)
    csf = carameldb.Caramel(
        keys, values, prefilter=prefilter, shared_filter=True, verbose=False
    )
    for key, row in zip(keys, values):
        assert csf.query(key) == list(row)

    save_file = str(tmp_path / "shared_filter.csf")
    csf.save(save_file)
    csf2 = carameldb.load(save_file)
    for key, row in zip(keys, values):
        assert csf2.query(key) == list(row)


def test_multiset_csf_shared_codebook_and_filter(tmp_path):
    num_rows = 500
    num_cols = 5
    keys = [f"key_{i}" for i in range(num_rows)]
    values = np.zeros((num_rows, num_cols), dtype=np.uint32)
    for i in range(num_rows // 5):
        values[i] = np.random.randint(1, 10, size=num_cols, dtype=np.uint32)

    prefilter = carameldb.BinaryFuseFilterConfig(fingerprint_bits=12)
    csf = carameldb.Caramel(
        keys, values, prefilter=prefilter,
        shared_codebook=True, shared_filter=True, verbose=False
    )
    for key, row in zip(keys, values):
        assert csf.query(key) == list(row)

    save_file = str(tmp_path / "shared_both.csf")
    csf.save(save_file)
    csf2 = carameldb.load(save_file)
    for key, row in zip(keys, values):
        assert csf2.query(key) == list(row)


def test_multiset_csf_shared_codebook_with_permute(tmp_path):
    num_rows = 500
    num_cols = 5
    keys = [f"key_{i}" for i in range(num_rows)]
    values = np.random.randint(0, 10, size=(num_rows, num_cols), dtype=np.uint32)

    csf = carameldb.Caramel(
        keys, values, permute=True, shared_codebook=True, verbose=False
    )
    # Permutation reorders columns, so we can't compare against original.
    # Verify round-trip: save/load produces the same query results.
    results = {key: csf.query(key) for key in keys}
    save_file = str(tmp_path / "shared_cb_permute.csf")
    csf.save(save_file)
    csf2 = carameldb.load(save_file)
    for key in keys:
        assert csf2.query(key) == results[key]


def test_shared_options_rejected_for_single_column():
    keys = ["a", "b", "c"]
    values = [1, 2, 3]
    with pytest.raises(ValueError, match="shared_codebook"):
        carameldb.Caramel(keys, values, shared_codebook=True)
    with pytest.raises(ValueError, match="shared_filter"):
        carameldb.Caramel(keys, values, shared_filter=True)


def test_backward_compat_default_settings(tmp_path):
    """New code with default settings produces loadable files."""
    num_rows = 200
    num_cols = 3
    keys = [f"key_{i}" for i in range(num_rows)]
    values = np.random.randint(0, 100, size=(num_rows, num_cols), dtype=np.uint32)

    csf = carameldb.Caramel(keys, values, verbose=False)
    save_file = str(tmp_path / "default.csf")
    csf.save(save_file)
    csf2 = carameldb.load(save_file)
    for key, row in zip(keys, values):
        assert csf2.query(key) == list(row)
