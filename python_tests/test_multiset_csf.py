import shutil

import caramel
import pytest

pytestmark = [pytest.mark.unit]


def test_multiset_csf():
    num_rows = 1000
    num_columns = 10
    keys = [f"key_{i}" for i in range(num_rows)]
    values = [[j + i for j in range(num_columns)] for i in range(num_rows)]

    csf = caramel.MultisetCSF(keys, values)

    for key, value in zip(keys, values):
        assert csf.query(key) == value

    save_file = "multiset.csf"
    csf.save(save_file)
    csf = caramel.load(save_file)
    shutil.rmtree(save_file)

    for key, value in zip(keys, values):
        assert csf.query(key) == value
