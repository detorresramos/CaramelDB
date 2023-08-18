import caramel
import pytest


@pytest.mark.unit
def test_csf():
    keys = ["key1", "key2", "key3"]
    values = [4, 5, 6]
    csf = caramel.CSF(keys, values)

    for key, value in zip(keys, values):
        assert csf.query(key) == value
