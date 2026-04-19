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


def test_multiset_csf_no_lookup_table():
    """With build_lookup_table=False, queries fall back to canonical decode."""
    num_rows = 500
    num_cols = 5
    keys = [f"key_{i}" for i in range(num_rows)]
    values = np.random.default_rng(42).integers(0, 50, size=(num_rows, num_cols), dtype=np.uint32)

    csf = carameldb.Caramel(keys, values, build_lookup_table=False, verbose=False)
    for key, row in zip(keys, values):
        assert csf.query(key) == list(row)


def test_multiset_csf_no_lut_save_load(tmp_path):
    """Save/load round-trip with build_lookup_table=False matches saved queries."""
    num_rows = 500
    num_cols = 5
    keys = [f"key_{i}" for i in range(num_rows)]
    values = np.random.default_rng(42).integers(0, 50, size=(num_rows, num_cols), dtype=np.uint32)

    csf = carameldb.Caramel(keys, values, build_lookup_table=False, verbose=False)
    save_file = str(tmp_path / "no_lut.csf")
    csf.save(save_file)
    csf2 = carameldb.load(save_file)
    for key, row in zip(keys, values):
        assert csf2.query(key) == list(row)


def test_multiset_csf_no_lut_shared_codebook():
    """Shared codebook + no LUT uses the streaming freq path and canonical decode."""
    num_rows = 500
    num_cols = 5
    keys = [f"key_{i}" for i in range(num_rows)]
    values = np.random.default_rng(42).integers(0, 10, size=(num_rows, num_cols), dtype=np.uint32)

    csf = carameldb.Caramel(
        keys, values, shared_codebook=True, build_lookup_table=False, verbose=False
    )
    for key, row in zip(keys, values):
        assert csf.query(key) == list(row)


def test_multiset_csf_no_lut_permute_shared_codebook():
    """Grid script's Column+SharedCB-like path (but also with permute) works end-to-end."""
    num_rows = 500
    num_cols = 8
    keys = np.arange(num_rows, dtype=np.uint32)
    values = np.random.default_rng(42).integers(0, 15, size=(num_rows, num_cols), dtype=np.uint32)
    values_copy = values.copy()  # for comparison since permute is in place

    csf = carameldb.Caramel(
        keys, values,
        permutation=carameldb.GlobalSortPermutationConfig(refinement_iterations=3),
        shared_codebook=True,
        build_lookup_table=False,
        verbose=False,
    )
    for i in range(num_rows):
        assert sorted(csf.query(int(keys[i]))) == sorted(values_copy[i].tolist())


def test_multiset_csf_permute_mutates_in_place():
    """Verify that permutation no longer copies values — the caller's array is mutated."""
    num_rows = 100
    num_cols = 5
    keys = [f"k{i}" for i in range(num_rows)]
    values = np.random.default_rng(42).integers(0, 10, size=(num_rows, num_cols), dtype=np.uint32)
    original = values.copy()

    carameldb.Caramel(
        keys, values,
        permutation=carameldb.GlobalSortPermutationConfig(refinement_iterations=2),
        verbose=False,
    )
    # At least one row should differ if permutation did anything non-trivial
    # (we don't assert specific positions — just that mutation happened).
    assert not np.array_equal(values, original), \
        "values array should be mutated in place by permutation"
    # Row multisets must still match (permutation only reorders within rows).
    for i in range(num_rows):
        assert sorted(values[i].tolist()) == sorted(original[i].tolist())


def test_multiset_csf_no_lut_mmap_values(tmp_path):
    """Simulate the grid script's mmap'd numpy input on a small scale."""
    num_rows = 500
    num_cols = 5
    npy_path = tmp_path / "values.npy"
    rng = np.random.default_rng(42)
    values_src = rng.integers(0, 20, size=(num_rows, num_cols), dtype=np.uint32)
    np.save(npy_path, values_src)

    # Non-permute path uses mmap_mode='r'.
    keys = np.arange(num_rows, dtype=np.uint32)
    values_r = np.load(npy_path, mmap_mode='r')
    csf = carameldb.Caramel(keys, values_r, build_lookup_table=False, verbose=False)
    for i in range(num_rows):
        assert csf.query(int(keys[i])) == values_src[i].tolist()

    # Permute path uses mmap_mode='c' (writable copy-on-write).
    values_c = np.load(npy_path, mmap_mode='c')
    csf2 = carameldb.Caramel(
        keys, values_c,
        permutation=carameldb.GlobalSortPermutationConfig(refinement_iterations=2),
        build_lookup_table=False,
        verbose=False,
    )
    for i in range(num_rows):
        assert sorted(csf2.query(int(keys[i]))) == sorted(values_src[i].tolist())


def test_shared_codebook_serialized_bytes(tmp_path):
    """The retroactive helper returns positive bytes for any non-trivial input."""
    num_rows = 1000
    num_cols = 20
    rng = np.random.default_rng(42)
    values = rng.integers(1, 500, size=(num_rows, num_cols), dtype=np.uint32)

    cb_bytes = carameldb.shared_codebook_serialized_bytes(values)
    assert cb_bytes > 0
    assert isinstance(cb_bytes, int)


def test_shared_codebook_save_is_smaller_than_private(tmp_path):
    """With the serialization fix, shared_codebook=True saves fewer bytes than
    shared_codebook=False (one codebook on disk, not M copies)."""
    num_rows = 2000
    num_cols = 30
    rng = np.random.default_rng(7)
    values = rng.integers(0, 300, size=(num_rows, num_cols), dtype=np.uint32)
    keys = np.arange(num_rows, dtype=np.uint32)

    def dir_size(path):
        return sum(p.stat().st_size for p in path.iterdir())

    csf_shared = carameldb.Caramel(keys, values, shared_codebook=True,
                                   build_lookup_table=False, verbose=False)
    shared_dir = tmp_path / "shared.csf"
    csf_shared.save(str(shared_dir))
    shared_total = dir_size(shared_dir)
    assert (shared_dir / "shared_codebook.bin").exists()

    csf_private = carameldb.Caramel(keys, values, shared_codebook=False,
                                    build_lookup_table=False, verbose=False)
    private_dir = tmp_path / "private.csf"
    csf_private.save(str(private_dir))
    private_total = dir_size(private_dir)
    assert not (private_dir / "shared_codebook.bin").exists()

    assert shared_total < private_total, \
        f"shared_total={shared_total} should be less than private_total={private_total}"


def test_shared_codebook_save_load_roundtrip(tmp_path):
    """Shared-codebook CSF must load correctly and return identical queries."""
    num_rows = 500
    num_cols = 15
    rng = np.random.default_rng(11)
    values = rng.integers(0, 100, size=(num_rows, num_cols), dtype=np.uint32)
    keys = np.arange(num_rows, dtype=np.uint32)

    csf = carameldb.Caramel(keys, values, shared_codebook=True,
                            build_lookup_table=False, verbose=False)
    save_dir = tmp_path / "roundtrip.csf"
    csf.save(str(save_dir))
    loaded = carameldb.load(str(save_dir))

    for i in range(num_rows):
        expected = sorted(values[i].tolist())
        actual = sorted(loaded.query(int(keys[i])))
        assert actual == expected, f"row {i}: expected {expected}, got {actual}"


def test_shared_codebook_serialized_bytes_accepts_mmap(tmp_path):
    """Helper must work on an mmap'd numpy array without copying."""
    rng = np.random.default_rng(7)
    values_src = rng.integers(0, 50, size=(500, 10), dtype=np.uint32)
    path = tmp_path / "vals.npy"
    np.save(path, values_src)

    values_mmap = np.load(path, mmap_mode='r')
    cb_bytes = carameldb.shared_codebook_serialized_bytes(values_mmap)
    assert cb_bytes > 0

    # Same input non-mmap should yield identical bytes (deterministic).
    cb_bytes_ram = carameldb.shared_codebook_serialized_bytes(values_src)
    assert cb_bytes == cb_bytes_ram


def test_shared_codebook_serialized_bytes_dtype():
    rng = np.random.default_rng(0)
    vals32 = rng.integers(0, 100, size=(200, 5), dtype=np.uint32)
    vals64 = vals32.astype(np.uint64)
    # Both dtypes should accept the call; uint64 codebook will typically be
    # larger because ordered_symbols stores 8-byte values.
    b32 = carameldb.shared_codebook_serialized_bytes(vals32)
    b64 = carameldb.shared_codebook_serialized_bytes(vals64)
    assert b32 > 0 and b64 > 0
    assert b64 > b32
