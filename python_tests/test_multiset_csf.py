import os

import carameldb
import pytest

pytestmark = [pytest.mark.unit]


def test_multiset_csf():
    num_rows = 1000
    num_columns = 10
    keys = [f"key_{i}" for i in range(num_rows)]
    values = [[j + i for j in range(num_columns)] for i in range(num_rows)]

    csf = carameldb.Caramel(keys, values)
    assert isinstance(csf, carameldb.MultisetCSFUint32)

    for key, value in zip(keys, values):
        assert csf.query(key) == value

    save_file = "multiset.csf"
    csf.save(save_file)
    csf = carameldb.load(save_file)
    os.remove(save_file)

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


def test_multiset_csf_permute():
    num_rows = 1000
    num_columns = 10
    keys = [f"key_{i}" for i in range(num_rows)]
    values = [[j + i for j in range(num_columns)] for i in range(num_rows)]

    csf_permute = carameldb.Caramel(keys, values, premute=True)
    csf_permute_filename = "csf_permute.csf"
    csf_permute.save(csf_permute_filename)

    csf_no_permute = carameldb.Caramel(keys, values, premute=False)
    csf_no_permute_filename = "csf_no_permute.csf"
    csf_no_permute.save(csf_no_permute_filename)

    assert os.path.getsize(csf_permute_filename) < os.path.getsize(csf_permute_filename)

    os.remove(csf_permute_filename)
    os.remove(csf_no_permute_filename)
