import os
import tempfile

import carameldb
import numpy as np
import pytest
from carameldb import (
    BinaryFuseFilterConfig,
    BinaryFusePreFilterUint32,
    BloomFilterConfig,
    BloomPreFilterUint32,
    XORFilterConfig,
    XORPreFilterUint32,
)
from test_csf import assert_all_correct, assert_simple_api_correct, gen_str_keys


@pytest.mark.parametrize("bloom_filter", [True, False])
def test_csf_inference_with_bloom_doesnt_change(bloom_filter):
    keys = gen_str_keys(1000)
    values = np.array([5 for _ in range(len(keys))])
    if bloom_filter:
        # Use explicit size and num_hashes
        assert_simple_api_correct(
            keys, values, prefilter=BloomFilterConfig(size=10000, num_hashes=7)
        )
    else:
        assert_simple_api_correct(keys, values, prefilter=None)


def test_bloom_filter_with_different_sizes():
    rows = 10000
    keys = gen_str_keys(rows)
    # Create data with 80% most common value
    num_most_common_element = int(rows * 0.8)
    other_elements = rows - num_most_common_element
    values = [10000 for _ in range(num_most_common_element)] + [
        i for i in range(other_elements)
    ]

    # Test with smaller bloom filter
    csf_small = carameldb.Caramel(
        keys, values, prefilter=BloomFilterConfig(size=1000, num_hashes=3)
    )
    small_filename = "small.csf"
    csf_small.save(small_filename)
    small_size = os.path.getsize(small_filename)

    # Test with larger bloom filter
    csf_large = carameldb.Caramel(
        keys, values, prefilter=BloomFilterConfig(size=100000, num_hashes=7)
    )
    large_filename = "large.csf"
    csf_large.save(large_filename)
    large_size = os.path.getsize(large_filename)

    # Larger bloom filter should result in larger file
    assert large_size > small_size

    assert_all_correct(keys, values, csf_small)
    assert_all_correct(keys, values, csf_large)

    os.remove(small_filename)
    os.remove(large_filename)


def test_filter_custom_num_hashes():
    rows = 10000
    keys = gen_str_keys(rows)
    # Create data with 80% most common value
    num_most_common_element = int(rows * 0.8)
    other_elements = rows - num_most_common_element
    values = [10000 for _ in range(num_most_common_element)] + [
        i for i in range(other_elements)
    ]

    # Test with custom size and num_hashes
    csf = carameldb.Caramel(
        keys, values, prefilter=BloomFilterConfig(size=50000, num_hashes=10), verbose=True
    )

    bf = csf.get_filter().get_bloom_filter()
    assert bf.num_hashes() == 10


def test_get_bloom_filter():
    keys = gen_str_keys(1000)
    values = np.array([5 for _ in range(900)] + [6] * 100)
    csf = carameldb.Caramel(
        keys, values, prefilter=BloomFilterConfig(size=10000, num_hashes=7)
    )

    prefilter = csf.get_filter()
    assert prefilter != None
    assert prefilter.get_most_common_value() == 5

    # Test that the filter contains the non-most-common keys
    for i in range(900, 1000):
        assert prefilter.contains(f"key{i}")

    filename = "prefilter.bin"
    prefilter.save(filename)
    assert os.path.getsize(filename) > 10  # assert non-degenerate filter

    # Load and verify
    loaded_filter = BloomPreFilterUint32.load(filename)
    assert loaded_filter.get_most_common_value() == 5

    # Verify loaded filter contains the same keys
    for i in range(900, 1000):
        assert loaded_filter.contains(f"key{i}")

    os.remove(filename)


def test_no_filter_without_config():
    keys = gen_str_keys(1000)
    values = np.array([i for i in range(len(keys))])
    csf = carameldb.Caramel(keys, values)
    assert csf.get_filter() is None


def test_xor_filter_with_fingerprint_bits():
    """Test that XorFilter works correctly with explicit fingerprint bits."""
    rows = 10000
    keys = gen_str_keys(rows)

    # Create data with very high alpha (95% most common value)
    num_most_common_element = int(rows * 0.95)
    other_elements = rows - num_most_common_element
    values = [10000 for _ in range(num_most_common_element)] + [
        i for i in range(other_elements)
    ]

    # Test with XOR filter (8-bit fingerprints)
    csf_xor = carameldb.Caramel(
        keys, values, prefilter=XORFilterConfig(fingerprint_bits=8), verbose=True
    )
    xor_filename = "xor.csf"
    csf_xor.save(xor_filename)
    xor_size = os.path.getsize(xor_filename)

    # Load back and verify save/load works correctly
    csf_xor_loaded = carameldb.load(xor_filename)
    assert_all_correct(keys, values, csf_xor_loaded)

    # Test without filter
    csf_no_filter = carameldb.Caramel(keys, values, prefilter=None)
    no_filter_filename = "no_filter.csf"
    csf_no_filter.save(no_filter_filename)
    no_filter_size = os.path.getsize(no_filter_filename)

    # With high alpha (95%), XOR filter should reduce size
    print(
        f"XOR filter size: {xor_size}, No filter size: {no_filter_size}, Difference: {xor_size - no_filter_size}"
    )
    assert (
        xor_size < no_filter_size
    ), f"XOR filter should be smaller: {xor_size} >= {no_filter_size}"

    # Verify correctness - this is the key requirement
    assert_all_correct(keys, values, csf_xor)
    assert_all_correct(keys, values, csf_no_filter)

    os.remove(xor_filename)
    os.remove(no_filter_filename)


def test_get_xor_filter():
    keys = gen_str_keys(1000)
    values = np.array([5 for _ in range(900)] + [6] * 100)
    csf = carameldb.Caramel(
        keys, values, prefilter=XORFilterConfig(fingerprint_bits=8)
    )

    prefilter = csf.get_filter()
    assert prefilter != None
    assert prefilter.get_most_common_value() == 5

    # Test that the filter contains the non-most-common keys
    for i in range(900, 1000):
        assert prefilter.contains(f"key{i}")

    filename = "xor_prefilter.bin"
    prefilter.save(filename)
    assert os.path.getsize(filename) > 10  # assert non-degenerate filter

    # Load and verify
    loaded_filter = XORPreFilterUint32.load(filename)
    assert loaded_filter.get_most_common_value() == 5

    # Verify loaded filter contains the same keys
    for i in range(900, 1000):
        assert loaded_filter.contains(f"key{i}")

    os.remove(filename)


def test_binary_fuse_filter_with_fingerprint_bits():
    """Test that BinaryFuseFilter works correctly with explicit fingerprint bits."""
    rows = 10000
    keys = gen_str_keys(rows)

    # Create data with very high alpha (95% most common value)
    num_most_common_element = int(rows * 0.95)
    other_elements = rows - num_most_common_element
    values = [10000 for _ in range(num_most_common_element)] + [
        i for i in range(other_elements)
    ]

    # Test with Binary Fuse filter (8-bit fingerprints)
    csf_bf = carameldb.Caramel(
        keys, values, prefilter=BinaryFuseFilterConfig(fingerprint_bits=8), verbose=True
    )
    bf_filename = "binary_fuse.csf"
    csf_bf.save(bf_filename)
    bf_size = os.path.getsize(bf_filename)

    # Load back and verify save/load works correctly
    csf_bf_loaded = carameldb.load(bf_filename)
    assert_all_correct(keys, values, csf_bf_loaded)

    # Test without filter
    csf_no_filter = carameldb.Caramel(keys, values, prefilter=None)
    no_filter_filename = "no_filter_bf.csf"
    csf_no_filter.save(no_filter_filename)
    no_filter_size = os.path.getsize(no_filter_filename)

    # With high alpha (95%), Binary Fuse filter should reduce size
    print(
        f"Binary Fuse filter size: {bf_size}, No filter size: {no_filter_size}, Difference: {bf_size - no_filter_size}"
    )
    assert (
        bf_size < no_filter_size
    ), f"Binary Fuse filter should be smaller: {bf_size} >= {no_filter_size}"

    # Verify correctness
    assert_all_correct(keys, values, csf_bf)
    assert_all_correct(keys, values, csf_no_filter)

    os.remove(bf_filename)
    os.remove(no_filter_filename)


def test_get_binary_fuse_filter():
    keys = gen_str_keys(1000)
    values = np.array([5 for _ in range(900)] + [6] * 100)
    csf = carameldb.Caramel(
        keys, values, prefilter=BinaryFuseFilterConfig(fingerprint_bits=8)
    )

    prefilter = csf.get_filter()
    assert prefilter != None
    assert prefilter.get_most_common_value() == 5

    # Test that the filter contains the non-most-common keys
    for i in range(900, 1000):
        assert prefilter.contains(f"key{i}")

    filename = "binary_fuse_prefilter.bin"
    prefilter.save(filename)
    assert os.path.getsize(filename) > 10  # assert non-degenerate filter

    # Load and verify
    loaded_filter = BinaryFusePreFilterUint32.load(filename)
    assert loaded_filter.get_most_common_value() == 5

    # Verify loaded filter contains the same keys
    for i in range(900, 1000):
        assert loaded_filter.contains(f"key{i}")

    os.remove(filename)


def test_filter_always_created_when_config_provided():
    """Test that filter is always created when config is provided, regardless of data distribution."""
    keys = gen_str_keys(1000)
    # All values are the same - traditionally this would skip filtering
    values = np.array([5 for _ in range(len(keys))])

    # With new API, filter should always be created
    csf = carameldb.Caramel(
        keys, values, prefilter=BloomFilterConfig(size=10000, num_hashes=7)
    )

    # Filter should exist (even though all values are the same)
    assert csf.get_filter() is not None


def test_xor_filter_fingerprint_bits_range():
    """Test that different fingerprint bit values work correctly."""
    keys = gen_str_keys(1000)
    values = np.array([5 for _ in range(800)] + [i for i in range(200)])

    # Test various fingerprint bit sizes
    for bits in [4, 8, 12, 16]:
        csf = carameldb.Caramel(
            keys, values, prefilter=XORFilterConfig(fingerprint_bits=bits)
        )
        assert_all_correct(keys, values, csf)


def test_binary_fuse_filter_fingerprint_bits_range():
    """Test that different fingerprint bit values work correctly for binary fuse."""
    keys = gen_str_keys(1000)
    values = np.array([5 for _ in range(800)] + [i for i in range(200)])

    # Test various fingerprint bit sizes
    for bits in [4, 8, 12, 16]:
        csf = carameldb.Caramel(
            keys, values, prefilter=BinaryFuseFilterConfig(fingerprint_bits=bits)
        )
        assert_all_correct(keys, values, csf)
