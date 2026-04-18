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
    """Ragged values should be handled by RaggedMultisetCSF."""
    keys = ["1", "2", "3"]
    values = [[1, 2], [2, 3], [3]]
    csf = carameldb.Caramel(keys, values, verbose=False)
    for i in range(len(keys)):
        assert sorted(csf.query(keys[i])) == sorted(values[i])


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

    def dir_size(path):
        return sum(p.stat().st_size for p in path.iterdir())

    csf_permute = carameldb.Caramel(keys, values, permutation=carameldb.EntropyPermutationConfig(), verbose=False)
    csf_permute_file = tmp_path / "csf_permute.csf"
    csf_permute.save(str(csf_permute_file))
    csf_permute_size = dir_size(csf_permute_file)

    csf_no_permute = carameldb.Caramel(keys, values, verbose=False)
    csf_no_permute_file = tmp_path / "csf_no_permute.csf"
    csf_no_permute.save(str(csf_no_permute_file))
    csf_no_permute_size = dir_size(csf_no_permute_file)

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

    csf = carameldb.MultisetCSFUint32(keys, values, permutation=carameldb.EntropyPermutationConfig(), verbose=False)
    for key, row in zip(keys, values):
        assert sorted(csf.query(key)) == sorted(row)


def test_multiset_string_permutation_raises():
    keys = ["a", "b", "c"]
    values = np.array([["x", "y"], ["a", "b"], ["c", "d"]])
    with pytest.raises(
        ValueError, match="'permutation' is not supported for variable-length string values."
    ):
        carameldb.MultisetCSFString(keys, values, permutation=carameldb.EntropyPermutationConfig())


def test_multiset_csf_per_column_filter(tmp_path):
    """Per-column filter (filter_config set, shared_filter=False)."""
    num_rows = 500
    num_cols = 5
    keys = [f"key_{i}" for i in range(num_rows)]
    values = np.zeros((num_rows, num_cols), dtype=np.uint32)
    for i in range(num_rows // 5):
        values[i] = np.random.default_rng(42).integers(1, 10, size=num_cols, dtype=np.uint32)

    prefilter = carameldb.BinaryFuseFilterConfig(fingerprint_bits=12)
    csf = carameldb.Caramel(
        keys, values, prefilter=prefilter, shared_filter=False, verbose=False
    )
    for key, row in zip(keys, values):
        assert csf.query(key) == list(row)

    save_file = str(tmp_path / "per_col_filter.csf")
    csf.save(save_file)
    csf2 = carameldb.load(save_file)
    for key, row in zip(keys, values):
        assert csf2.query(key) == list(row)


def test_multiset_csf_shared_codebook(tmp_path):
    num_rows = 500
    num_cols = 5
    keys = [f"key_{i}" for i in range(num_rows)]
    values = np.random.default_rng(42).integers(0, 10, size=(num_rows, num_cols), dtype=np.uint32)

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
    rng = np.random.default_rng(42)
    values = np.zeros((num_rows, num_cols), dtype=np.uint32)
    for i in range(num_rows // 5):
        values[i] = rng.integers(1, 10, size=num_cols, dtype=np.uint32)

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


def test_multiset_csf_shared_filter_multi_group(tmp_path):
    """Columns with different MCVs get separate filter groups."""
    num_rows = 500
    num_cols = 4
    keys = [f"key_{i}" for i in range(num_rows)]
    values = np.zeros((num_rows, num_cols), dtype=np.uint32)
    # Columns 0,1 have MCV=0; columns 2,3 have MCV=7
    values[:, 2:] = 7
    # Sprinkle minority values in all columns
    rng = np.random.default_rng(42)
    for i in rng.choice(num_rows, size=num_rows // 5, replace=False):
        values[i] = rng.integers(1, 10, size=num_cols, dtype=np.uint32)

    prefilter = carameldb.BinaryFuseFilterConfig(fingerprint_bits=12)
    csf = carameldb.Caramel(
        keys, values, prefilter=prefilter, shared_filter=True, verbose=False
    )
    for key, row in zip(keys, values):
        assert csf.query(key) == list(row)

    save_file = str(tmp_path / "shared_filter_multi_group.csf")
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
        values[i] = np.random.default_rng(42).integers(1, 10, size=num_cols, dtype=np.uint32)

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


def test_multiset_csf_shared_codebook_with_permutation(tmp_path):
    num_rows = 500
    num_cols = 5
    keys = [f"key_{i}" for i in range(num_rows)]
    values = np.random.default_rng(42).integers(0, 10, size=(num_rows, num_cols), dtype=np.uint32)

    csf = carameldb.Caramel(
        keys, values, permutation=carameldb.EntropyPermutationConfig(),
        shared_codebook=True, verbose=False
    )
    results = {key: csf.query(key) for key in keys}
    save_file = str(tmp_path / "shared_cb_permute.csf")
    csf.save(save_file)
    csf2 = carameldb.load(save_file)
    for key in keys:
        assert csf2.query(key) == results[key]


def test_multiset_csf_global_sort_permutation(tmp_path):
    num_rows = 1000
    num_columns = 10
    keys = [f"key_{i}" for i in range(num_rows)]
    values = []
    for _ in range(num_rows):
        lst = [j for j in range(num_columns)]
        random.shuffle(lst)
        values.append(lst)
    values = np.array(values)

    csf = carameldb.Caramel(
        keys, values,
        permutation=carameldb.GlobalSortPermutationConfig(refinement_iterations=3),
        verbose=False,
    )
    save_file = str(tmp_path / "global_sort.csf")
    csf.save(save_file)
    csf2 = carameldb.load(save_file)
    results = {key: csf.query(key) for key in keys}
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
    values = np.random.default_rng(42).integers(0, 100, size=(num_rows, num_cols), dtype=np.uint32)

    csf = carameldb.Caramel(keys, values, verbose=False)
    save_file = str(tmp_path / "default.csf")
    csf.save(save_file)
    csf2 = carameldb.load(save_file)
    for key, row in zip(keys, values):
        assert csf2.query(key) == list(row)
