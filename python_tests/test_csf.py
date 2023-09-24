import os

import caramel
import pytest


@pytest.mark.unit
def test_csf():
    keys = [f"key{i}" for i in range(1000)]
    values = [i for i in range(1000)]
    csf = caramel.CSF(keys, values)

    for key, value in zip(keys, values):
        assert csf.query(key) == value

    filename = "temp.csf"
    csf.save(filename)
    csf = caramel.CSF.load(filename)

    for key, value in zip(keys, values):
        assert csf.query(key) == value

    os.remove(filename)
