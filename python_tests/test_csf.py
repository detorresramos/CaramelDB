import os

import caramel
import pytest


def run_csf_int_test(keys, values):
    csf = caramel.CSFUint32(keys, values)

    for key, value in zip(keys, values):
        assert csf.query(key) == value

    filename = "temp.csf"
    csf.save(filename)
    csf = caramel.CSFUint32.load(filename)

    for key, value in zip(keys, values):
        assert csf.query(key) == value

    os.remove(filename)


@pytest.mark.unit
def test_csf_int():
    keys = [f"key{i}" for i in range(1000)]
    values = [i for i in range(1000)]
    run_csf_int_test(keys, values)


@pytest.mark.unit
def test_csf_byte_string_keys():
    keys = [f"key{i}".encode("utf-8") for i in range(1000)]
    values = [i for i in range(1000)]
    run_csf_int_test(keys, values)


@pytest.mark.unit
def test_csf_char_10():
    keys = [f"key{i}".encode("utf-8") for i in range(1000)]
    values = [str(f"value{i}".ljust(10)[:10]) for i in range(1000)]

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


@pytest.mark.unit
def test_csf_load_incorrect_type_fails():
    filename = "temp.csf"
    with pytest.raises(caramel.CsfDeserializationException) as e:
        keys = [f"key{i}".encode("utf-8") for i in range(1000)]
        values = [f"value{i}" for i in range(1000)]
        csf = caramel.CSFString(keys, values)
        csf.save(filename)
        csf = caramel.CSFUint32.load(filename)
    os.remove(filename)


@pytest.mark.unit
def test_auto_infer_char10():
    keys = [f"key{i}".encode("utf-8") for i in range(1000)]
    values = [str(f"value{i}".ljust(10)[:10]) for i in range(1000)]
    csf = caramel.CSF(keys, values)
    assert isinstance(csf, caramel.CSFChar10)
    filename = "temp.csf"
    csf.save(filename)
    csf = caramel.load(filename)
    assert isinstance(csf, caramel.CSFChar10)
    os.remove(filename)
    

def test_auto_infer_char12():
    keys = [f"key{i}".encode("utf-8") for i in range(1000)]
    values = [str(f"value{i}".ljust(12)[:12]) for i in range(1000)]
    csf = caramel.CSF(keys, values)
    assert isinstance(csf, caramel.CSFChar12)
    filename = "temp.csf"
    csf.save(filename)
    csf = caramel.load(filename)
    assert isinstance(csf, caramel.CSFChar12)
    os.remove(filename)


def test_auto_infer_string():
    keys = [f"key{i}".encode("utf-8") for i in range(1000)]
    values = [f"value{i}" for i in range(1000)]
    csf = caramel.CSF(keys, values)
    assert isinstance(csf, caramel.CSFString)
    filename = "temp.csf"
    csf.save(filename)
    csf = caramel.load(filename)
    assert isinstance(csf, caramel.CSFString)
    os.remove(filename)


def test_auto_infer_int():
    keys = [f"key{i}".encode("utf-8") for i in range(1000)]
    values = [i for i in range(1000)]
    csf = caramel.CSF(keys, values)
    assert isinstance(csf, caramel.CSFUint32)
    filename = "temp.csf"
    csf.save(filename)
    csf = caramel.load(filename)
    assert isinstance(csf, caramel.CSFUint32)
    os.remove(filename)
