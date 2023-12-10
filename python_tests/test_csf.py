import os

import caramel
import pytest

pytestmark = [pytest.mark.unit]

gen_str_keys = lambda n: [f"key{i}" for i in range(n)]
gen_byte_keys = lambda n: [f"key{i}".encode("utf-8") for i in range(n)]
gen_int_values = lambda n: [i for i in range(n)]
gen_charX_values = lambda n, x: [str(f"v{i}".ljust(x)[:x]) for i in range(n)]
gen_str_values = lambda n: [f"value{i}" for i in range(n)]


def assert_all_correct(keys, values, csf):
    for key, value in zip(keys, values):
        assert csf.query(key) == value


def assert_build_save_load_correct(keys, values, CSFClass, wrap_fn=None):
    csf = CSFClass(keys, values)
    if wrap_fn:
        csf = wrap_fn(csf)
    assert_all_correct(keys, values, csf)
    filename = "temp.csf"
    csf.save(filename)
    csf = CSFClass.load(filename)
    if wrap_fn:
        csf = wrap_fn(csf)
    assert_all_correct(keys, values, csf)
    os.remove(filename)


def assert_simple_api_correct(keys, values):
    csf = caramel.CSF(keys, values)
    assert_all_correct(keys, values, csf)
    filename = "temp.csf"
    csf.save(filename)
    csf = caramel.load(filename)
    assert_all_correct(keys, values, csf)
    os.remove(filename)


def test_csf_int():
    keys = gen_str_keys(1000)
    values = gen_int_values(1000)
    assert_build_save_load_correct(keys, values, caramel.CSFUint32)


def test_byte_keys():
    keys = gen_byte_keys(1000)
    values = gen_int_values(1000)
    assert_build_save_load_correct(keys, values, caramel.CSFUint32)


@pytest.mark.unit
def test_csf_char_10():
    keys = gen_byte_keys(1000)
    values = gen_charX_values(1000, 10)
    wrap_fn = lambda csf: caramel.CSFQueryWrapper(csf, lambda x: "".join(x))
    assert_build_save_load_correct(keys, values, caramel.CSFChar10, wrap_fn)


def test_csf_char_12():
    keys = gen_byte_keys(1000)
    values = gen_charX_values(1000, 12)
    wrap_fn = lambda csf: caramel.CSFQueryWrapper(csf, lambda x: "".join(x))
    assert_build_save_load_correct(keys, values, caramel.CSFChar12, wrap_fn)


def test_csf_string():
    keys = gen_byte_keys(1000)
    values = gen_str_values(1000)
    assert_build_save_load_correct(keys, values, caramel.CSFString)


def test_csf_load_incorrect_type_fails():
    filename = "temp.csf"
    with pytest.raises(caramel.CsfDeserializationException) as e:
        keys = [f"key{i}".encode("utf-8") for i in range(1000)]
        values = [f"value{i}" for i in range(1000)]
        csf = caramel.CSFString(keys, values)
        csf.save(filename)
        csf = caramel.CSFUint32.load(filename)
    os.remove(filename)


def test_auto_infer_char10():
    keys = gen_byte_keys(1000)
    values = gen_charX_values(1000, 10)
    assert caramel._infer_backend(keys, values) == caramel.CSFChar10


def test_auto_infer_char12():
    keys = gen_byte_keys(1000)
    values = gen_charX_values(1000, 12)
    assert caramel._infer_backend(keys, values) == caramel.CSFChar12


def test_auto_infer_string():
    keys = gen_str_keys(1000)
    values = gen_str_values(1000)
    assert caramel._infer_backend(keys, values) == caramel.CSFString


def test_auto_infer_bytes():
    keys = gen_str_keys(1000)
    values = [f"V{i}".encode("utf-8") for i in range(1000)]
    assert caramel._infer_backend(keys, values) == caramel.CSFString


def test_auto_infer_int():
    keys = gen_str_keys(1000)
    values = gen_int_values(1000)
    assert caramel._infer_backend(keys, values) == caramel.CSFUint32


def test_end_to_end():
    # Tests the full backend-inference + wrapper.
    num_elements = 100
    keys = gen_str_keys(num_elements)
    value_sets = [
        gen_int_values(num_elements),
        gen_str_values(num_elements),
        gen_charX_values(num_elements, 10),
        gen_charX_values(num_elements, 12),
    ]
    for values in value_sets:
        assert_simple_api_correct(keys, values)
