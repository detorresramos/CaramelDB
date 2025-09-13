import os
import tempfile

import carameldb
import numpy as np
import pytest
from carameldb import BloomFilterConfig, XORFilterConfig
from test_csf import assert_all_correct, assert_simple_api_correct, gen_str_keys


@pytest.mark.parametrize("bloom_filter", [True, False])
def test_csf_inference_with_bloom_doesnt_change(bloom_filter):
    keys = gen_str_keys(1000)
    values = np.array([5 for _ in range(len(keys))])
    if bloom_filter:
        assert_simple_api_correct(keys, values, prefilter=BloomFilterConfig())
    else:
        assert_simple_api_correct(keys, values, prefilter=None)


def test_bloom_filter_with_custom_error_rate():
    rows = 10000
    keys = gen_str_keys(rows)
    # Create data with 80% most common value
    num_most_common_element = int(rows * 0.8)
    other_elements = rows - num_most_common_element
    values = [10000 for _ in range(num_most_common_element)] + [
        i for i in range(other_elements)
    ]

    # Test with custom error rate
    csf_custom_error = carameldb.Caramel(
        keys, values, prefilter=BloomFilterConfig(error_rate=0.01)
    )
    custom_filename = "custom_error.csf"
    csf_custom_error.save(custom_filename)
    custom_size = os.path.getsize(custom_filename)

    # Test with higher error rate (smaller filter)
    csf_high_error = carameldb.Caramel(
        keys, values, prefilter=BloomFilterConfig(error_rate=0.1)
    )
    high_error_filename = "high_error.csf"
    csf_high_error.save(high_error_filename)
    high_error_size = os.path.getsize(high_error_filename)

    # Lower error rate should result in larger bloom filter
    assert custom_size > high_error_size

    assert_all_correct(keys, values, csf_custom_error)
    assert_all_correct(keys, values, csf_high_error)

    os.remove(custom_filename)
    os.remove(high_error_filename)


def test_filter_custom_k():
    rows = 10000
    keys = gen_str_keys(rows)
    # Create data with 80% most common value
    num_most_common_element = int(rows * 0.8)
    other_elements = rows - num_most_common_element
    values = [10000 for _ in range(num_most_common_element)] + [
        i for i in range(other_elements)
    ]

    # Test with custom error rate
    csf = carameldb.Caramel(
        keys, values, prefilter=BloomFilterConfig(k=10000), verbose=True
    )

    bf = csf.get_filter().get_bloom_filter()

    for i in range(10000):
        assert bf.contains(str(i))


def test_get_bloom_filter():
    keys = gen_str_keys(1000)
    values = np.array([5 for _ in range(900)] + [6] * 100)
    csf = carameldb.Caramel(keys, values, prefilter=BloomFilterConfig())

    prefilter = csf.get_filter()
    assert prefilter != None
    assert prefilter.get_most_common_value() == 5

    filename = "prefilter.bin"
    prefilter.save(filename)
    assert os.path.getsize(filename) > 10  # assert non-degenerate filter
    os.remove(filename)


def test_no_bloom_filter_created():
    keys = gen_str_keys(1000)
    values = np.array([i for i in range(len(keys))])
    csf = carameldb.Caramel(keys, values)
    assert csf.get_filter() is None


@pytest.mark.parametrize(
    "most_common_frequency",
    [0.3, 0.5, 0.7, 0.78, 0.8, 0.9, 1.0],
)
def test_bloom_filter_threshold(most_common_frequency):
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
        # the small difference of 51 bytes is because we still make and save the
        # empty object despite not using it
        assert bloom_size - 51 == no_bloom_size
    else:
        assert bloom_size < no_bloom_size

    os.remove(bloom_filename)
    os.remove(no_bloom_filename)
