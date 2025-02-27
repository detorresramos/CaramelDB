import os

import numpy as np
import pytest

import carameldb
from carameldb.filters import BloomFilterConfig, XORFilterConfig

gen_str_keys = lambda n: [f"key{i}" for i in range(n)]
gen_byte_keys = lambda n: [f"key{i}".encode("utf-8") for i in range(n)]
gen_int_values = lambda n: np.array([i for i in range(n)])
gen_charX_values = lambda n, x: np.array([str(f"v{i}".ljust(x)[:x]) for i in range(n)])
gen_str_values = lambda n: np.array([f"value{i}" for i in range(n)])


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


def assert_simple_api_correct(keys, values, prefilter=None):
    csf = carameldb.Caramel(keys, values, prefilter=prefilter)
    assert_all_correct(keys, values, csf)
    filename = "temp.csf"
    csf.save(filename)
    csf = carameldb.load(filename)
    assert_all_correct(keys, values, csf)
    os.remove(filename)


def test_csf_int():
    keys = gen_str_keys(1000)
    values = gen_int_values(1000)
    assert_build_save_load_correct(keys, values, carameldb.CSFUint32)


def test_byte_keys():
    keys = gen_byte_keys(1000)
    values = gen_int_values(1000)
    assert_build_save_load_correct(keys, values, carameldb.CSFUint32)


def test_csf_char_10():
    keys = gen_byte_keys(1000)
    values = gen_charX_values(1000, 10)
    wrap_fn = lambda csf: carameldb.CSFQueryWrapper(csf, lambda x: "".join(x))
    assert_build_save_load_correct(keys, values, carameldb.CSFChar10, wrap_fn)


def test_csf_char_12():
    keys = gen_byte_keys(1000)
    values = gen_charX_values(1000, 12)
    wrap_fn = lambda csf: carameldb.CSFQueryWrapper(csf, lambda x: "".join(x))
    assert_build_save_load_correct(keys, values, carameldb.CSFChar12, wrap_fn)


def test_csf_string():
    keys = gen_byte_keys(1000)
    values = gen_str_values(1000)
    assert_build_save_load_correct(keys, values, carameldb.CSFString)


def test_csf_load_incorrect_type_fails():
    filename = "temp.csf"
    with pytest.raises(carameldb.CsfDeserializationException) as e:
        keys = [f"key{i}".encode("utf-8") for i in range(1000)]
        values = [f"value{i}" for i in range(1000)]
        csf = carameldb.CSFString(keys, values)
        csf.save(filename)
        csf = carameldb.CSFUint32.load(filename)
    os.remove(filename)


def test_auto_infer_char10():
    keys = gen_byte_keys(1000)
    values = gen_charX_values(1000, 10)
    assert carameldb._infer_backend(values) == carameldb.CSFChar10


def test_auto_infer_char12():
    keys = gen_byte_keys(1000)
    values = gen_charX_values(1000, 12)
    assert carameldb._infer_backend(values) == carameldb.CSFChar12


def test_auto_infer_string():
    keys = gen_str_keys(1000)
    values = gen_str_values(1000)
    assert carameldb._infer_backend(values) == carameldb.CSFString


def test_auto_infer_bytes():
    keys = gen_str_keys(1000)
    values = np.array([f"V{i}".encode("utf-8") for i in range(1000)])
    assert carameldb._infer_backend(values) == carameldb.CSFString


def test_auto_infer_int():
    keys = gen_str_keys(1000)
    values = gen_int_values(1000)
    assert carameldb._infer_backend(values) == carameldb.CSFUint32


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


@pytest.mark.parametrize(
    "most_common_frequency",
    [0.3, 0.5, 0.7, 0.78, 0.8, 0.9, 1.0],
)
def test_bloom_filter(most_common_frequency):
    rows = 10000
    keys = gen_str_keys(rows)
    num_most_common_element = int(rows * most_common_frequency)
    other_elements = rows - num_most_common_element
    values = [10000 for _ in range(num_most_common_element)] + [
        i for i in range(other_elements)
    ]

    csf_bloom = carameldb.Caramel(keys, values, prefilter=BloomFilterConfig())
    bloom_filename = "bloom.csf"
    csf_bloom.save(bloom_filename)
    bloom_size = os.path.getsize(bloom_filename)

    csf_no_bloom = carameldb.Caramel(keys, values, prefilter=None)
    no_bloom_filename = "no_bloom.csf"
    csf_no_bloom.save(no_bloom_filename)
    no_bloom_size = os.path.getsize(no_bloom_filename)

    if most_common_frequency < 0.79 or most_common_frequency == 1.0:
        # the small difference of 50 bytes is because we still make and save the
        # empty object despite not using it
        assert bloom_size - 50 == no_bloom_size
    else:
        assert bloom_size < no_bloom_size

    os.remove(bloom_filename)
    os.remove(no_bloom_filename)


def test_uint32_vs_64_values():
    uint32_t_values = np.array([1, 2, 3], dtype=np.uint32)
    assert carameldb._infer_backend(uint32_t_values) == carameldb.CSFUint32
    uint64_t_values = np.array([1, 2, 3], dtype=np.uint64)
    assert carameldb._infer_backend(uint64_t_values) == carameldb.CSFUint64


@pytest.mark.parametrize("bloom_filter", [True, False])
def test_all_same_with_and_without_bloom(bloom_filter):
    keys = gen_str_keys(1000)
    values = np.array([5 for i in range(len(keys))])
    if bloom_filter:
        assert_simple_api_correct(keys, values, prefilter=BloomFilterConfig())
    else:
        assert_simple_api_correct(keys, values, prefilter=None)


def test_unsolvable():
    keys = ["1", "2", "3", "4", "4"]
    values = [1, 2, 3, 4, 5]

    with pytest.raises(
        RuntimeError,
        match="Detected a key collision under 128-bit hash. Likely due to a duplicate key.",
    ):
        carameldb.Caramel(keys, values)
