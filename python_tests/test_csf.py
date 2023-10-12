import os

import caramel
import pytest


def run_csf_test(keys, values):
    csf = caramel.CSF(keys, values)

    for key, value in zip(keys, values):
        assert csf.query(key) == value

    filename = "temp.csf"
    csf.save(filename)
    csf = caramel.CSF.load(filename)

    for key, value in zip(keys, values):
        assert csf.query(key) == value

    os.remove(filename)


@pytest.mark.unit
def test_csf():
    keys = [f"key{i}" for i in range(1000)]
    values = [i for i in range(1000)]
    run_csf_test(keys, values)


@pytest.mark.unit
def test_csf_byte_strings():
    keys = [f"key{i}".encode("utf-8") for i in range(1000)]
    values = [i for i in range(1000)]
    run_csf_test(keys, values)


@pytest.mark.unit
def test_csf_char_10():
    keys = [f"key{i}".encode("utf-8") for i in range(1000)]
    values = [list(f"value{i}".ljust(10)[:10]) for i in range(1000)]

    csf = caramel.CSFChar10(keys, values)

    for key, value in zip(keys, values):
        assert csf.query(key) == value

    filename = "temp.csf"
    csf.save(filename)
    csf = caramel.CSFChar10.load(filename)

    for key, value in zip(keys, values):
        assert csf.query(key) == value

    os.remove(filename)



@pytest.mark.unit
def test_csf_string():
    keys = [f"key{i}".encode("utf-8") for i in range(1000)]
    values = [f"value{i}" for i in range(1000)]
    
    csf = caramel.CSFString(keys, values)

    for key, value in zip(keys, values):
        assert csf.query(key) == value

    filename = "temp.csf"
    csf.save(filename)
    csf = caramel.CSFString.load(filename)

    for key, value in zip(keys, values):
        assert csf.query(key) == value

    os.remove(filename)
