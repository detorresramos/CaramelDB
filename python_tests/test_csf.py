import carameldb
import numpy as np
import pytest
from conftest import (
    assert_all_correct,
    assert_build_save_load_correct,
    assert_simple_api_correct,
    gen_byte_keys,
    gen_charX_values,
    gen_int_values,
    gen_str_keys,
    gen_str_values,
)


def test_csf_int(tmp_path):
    keys = gen_str_keys(1000)
    values = gen_int_values(1000)
    assert_build_save_load_correct(keys, values, carameldb.CSFUint32, tmp_path)


def test_byte_keys(tmp_path):
    keys = gen_byte_keys(1000)
    values = gen_int_values(1000)
    assert_build_save_load_correct(keys, values, carameldb.CSFUint32, tmp_path)


def test_csf_char_10(tmp_path):
    keys = gen_byte_keys(1000)
    values = gen_charX_values(1000, 10)
    assert_build_save_load_correct(keys, values, carameldb.CSFChar10, tmp_path)


def test_csf_char_12(tmp_path):
    keys = gen_byte_keys(1000)
    values = gen_charX_values(1000, 12)
    assert_build_save_load_correct(keys, values, carameldb.CSFChar12, tmp_path)


def test_csf_string(tmp_path):
    keys = gen_byte_keys(1000)
    values = gen_str_values(1000)
    assert_build_save_load_correct(keys, values, carameldb.CSFString, tmp_path)


def test_csf_load_incorrect_type_fails(tmp_path):
    filename = str(tmp_path / "temp.csf")
    with pytest.raises(carameldb.CsfDeserializationException) as e:
        keys = [f"key{i}".encode("utf-8") for i in range(1000)]
        values = [f"value{i}" for i in range(1000)]
        csf = carameldb.CSFString(keys, values)
        csf.save(filename)
        csf = carameldb.CSFUint32.load(filename)


def test_auto_infer_char10():
    values = gen_charX_values(1000, 10)
    assert carameldb._infer_backend(values) == carameldb.CSFChar10


def test_auto_infer_char12():
    values = gen_charX_values(1000, 12)
    assert carameldb._infer_backend(values) == carameldb.CSFChar12


def test_auto_infer_string():
    values = gen_str_values(1000)
    assert carameldb._infer_backend(values) == carameldb.CSFString


def test_auto_infer_bytes():
    values = np.array([f"V{i}".encode("utf-8") for i in range(1000)])
    assert carameldb._infer_backend(values) == carameldb.CSFString


def test_auto_infer_int():
    values = gen_int_values(1000)
    assert carameldb._infer_backend(values) == carameldb.CSFUint32


def test_end_to_end(tmp_path):
    num_elements = 100
    keys = gen_str_keys(num_elements)
    value_sets = [
        gen_int_values(num_elements),
        gen_str_values(num_elements),
        gen_charX_values(num_elements, 10),
        gen_charX_values(num_elements, 12),
    ]
    for values in value_sets:
        assert_simple_api_correct(keys, values, tmp_path)


def test_uint32_vs_64_values():
    uint32_t_values = np.array([1, 2, 3], dtype=np.uint32)
    assert carameldb._infer_backend(uint32_t_values) == carameldb.CSFUint32
    uint64_t_values = np.array([1, 2, 3], dtype=np.uint64)
    assert carameldb._infer_backend(uint64_t_values) == carameldb.CSFUint64


def test_infer_backend_negative_raises():
    values = np.array([-1, 2, 3])
    with pytest.raises(ValueError, match="Negative integer values are not supported"):
        carameldb._infer_backend(values)


def test_infer_backend_large_int64_uses_uint64():
    values = np.array([0, 2**33])
    assert carameldb._infer_backend(values) == carameldb.CSFUint64


def test_infer_backend_small_int64_uses_uint32():
    values = np.array([0, 1, 2])
    assert carameldb._infer_backend(values) == carameldb.CSFUint32


def test_unsolvable():
    keys = ["1", "2", "3", "4", "4"]
    values = [1, 2, 3, 4, 5]

    with pytest.raises(
        RuntimeError,
        match="Detected a key collision under 128-bit hash. Likely due to a duplicate key.",
    ):
        carameldb.Caramel(keys, values)
