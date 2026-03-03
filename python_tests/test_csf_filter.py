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
from conftest import assert_all_correct, assert_simple_api_correct, gen_str_keys


@pytest.mark.parametrize("bloom_filter", [True, False])
def test_csf_inference_with_bloom_doesnt_change(bloom_filter, tmp_path):
    keys = gen_str_keys(1000)
    values = np.array([5 for _ in range(len(keys))])
    if bloom_filter:
        assert_simple_api_correct(
            keys, values, tmp_path, prefilter=BloomFilterConfig(bits_per_element=10, num_hashes=7)
        )
    else:
        assert_simple_api_correct(keys, values, tmp_path, prefilter=None)


def test_bloom_filter_with_different_sizes(tmp_path):
    rows = 10000
    keys = gen_str_keys(rows)
    num_most_common_element = int(rows * 0.8)
    other_elements = rows - num_most_common_element
    values = [10000 for _ in range(num_most_common_element)] + [
        i for i in range(other_elements)
    ]

    csf_small = carameldb.Caramel(
        keys, values, prefilter=BloomFilterConfig(bits_per_element=1, num_hashes=3)
    )
    small_file = tmp_path / "small.csf"
    csf_small.save(str(small_file))
    small_size = small_file.stat().st_size

    csf_large = carameldb.Caramel(
        keys, values, prefilter=BloomFilterConfig(bits_per_element=50, num_hashes=7)
    )
    large_file = tmp_path / "large.csf"
    csf_large.save(str(large_file))
    large_size = large_file.stat().st_size

    assert large_size > small_size

    assert_all_correct(keys, values, csf_small)
    assert_all_correct(keys, values, csf_large)


def test_filter_custom_num_hashes():
    rows = 10000
    keys = gen_str_keys(rows)
    num_most_common_element = int(rows * 0.8)
    other_elements = rows - num_most_common_element
    values = [10000 for _ in range(num_most_common_element)] + [
        i for i in range(other_elements)
    ]

    csf = carameldb.Caramel(
        keys,
        values,
        prefilter=BloomFilterConfig(bits_per_element=25, num_hashes=10),
        verbose=True,
    )

    stats = csf.get_filter().get_stats()
    assert stats.num_hashes == 10


def test_get_bloom_filter(tmp_path):
    keys = gen_str_keys(1000)
    values = np.array([5 for _ in range(900)] + [6] * 100)
    csf = carameldb.Caramel(
        keys, values, prefilter=BloomFilterConfig(bits_per_element=10, num_hashes=7)
    )

    prefilter = csf.get_filter()
    assert prefilter != None
    assert prefilter.get_most_common_value() == 5

    for i in range(900, 1000):
        assert prefilter.contains(f"key{i}")

    filename = str(tmp_path / "prefilter.bin")
    prefilter.save(filename)
    assert (tmp_path / "prefilter.bin").stat().st_size > 10

    loaded_filter = BloomPreFilterUint32.load(filename)
    assert loaded_filter.get_most_common_value() == 5

    for i in range(900, 1000):
        assert loaded_filter.contains(f"key{i}")


def test_no_filter_without_config():
    keys = gen_str_keys(1000)
    values = np.array([i for i in range(len(keys))])
    csf = carameldb.Caramel(keys, values)
    assert csf.get_filter() is None


def test_xor_filter_with_fingerprint_bits(tmp_path):
    """Test that XorFilter works correctly with explicit fingerprint bits."""
    rows = 10000
    keys = gen_str_keys(rows)

    num_most_common_element = int(rows * 0.95)
    other_elements = rows - num_most_common_element
    values = [10000 for _ in range(num_most_common_element)] + [
        i for i in range(other_elements)
    ]

    csf_xor = carameldb.Caramel(
        keys, values, prefilter=XORFilterConfig(fingerprint_bits=8), verbose=True
    )
    xor_file = tmp_path / "xor.csf"
    csf_xor.save(str(xor_file))
    xor_size = xor_file.stat().st_size

    csf_xor_loaded = carameldb.load(str(xor_file))
    assert_all_correct(keys, values, csf_xor_loaded)

    csf_no_filter = carameldb.Caramel(keys, values, prefilter=None)
    no_filter_file = tmp_path / "no_filter.csf"
    csf_no_filter.save(str(no_filter_file))
    no_filter_size = no_filter_file.stat().st_size

    print(
        f"XOR filter size: {xor_size}, No filter size: {no_filter_size}, Difference: {xor_size - no_filter_size}"
    )
    assert (
        xor_size < no_filter_size
    ), f"XOR filter should be smaller: {xor_size} >= {no_filter_size}"

    assert_all_correct(keys, values, csf_xor)
    assert_all_correct(keys, values, csf_no_filter)


def test_get_xor_filter(tmp_path):
    keys = gen_str_keys(1000)
    values = np.array([5 for _ in range(900)] + [6] * 100)
    csf = carameldb.Caramel(keys, values, prefilter=XORFilterConfig(fingerprint_bits=8))

    prefilter = csf.get_filter()
    assert prefilter != None
    assert prefilter.get_most_common_value() == 5

    for i in range(900, 1000):
        assert prefilter.contains(f"key{i}")

    filename = str(tmp_path / "xor_prefilter.bin")
    prefilter.save(filename)
    assert (tmp_path / "xor_prefilter.bin").stat().st_size > 10

    loaded_filter = XORPreFilterUint32.load(filename)
    assert loaded_filter.get_most_common_value() == 5

    for i in range(900, 1000):
        assert loaded_filter.contains(f"key{i}")


def test_binary_fuse_filter_with_fingerprint_bits(tmp_path):
    """Test that BinaryFuseFilter works correctly with explicit fingerprint bits."""
    rows = 10000
    keys = gen_str_keys(rows)

    num_most_common_element = int(rows * 0.95)
    other_elements = rows - num_most_common_element
    values = [10000 for _ in range(num_most_common_element)] + [
        i for i in range(other_elements)
    ]

    csf_bf = carameldb.Caramel(
        keys, values, prefilter=BinaryFuseFilterConfig(fingerprint_bits=8), verbose=True
    )
    bf_file = tmp_path / "binary_fuse.csf"
    csf_bf.save(str(bf_file))
    bf_size = bf_file.stat().st_size

    csf_bf_loaded = carameldb.load(str(bf_file))
    assert_all_correct(keys, values, csf_bf_loaded)

    csf_no_filter = carameldb.Caramel(keys, values, prefilter=None)
    no_filter_file = tmp_path / "no_filter_bf.csf"
    csf_no_filter.save(str(no_filter_file))
    no_filter_size = no_filter_file.stat().st_size

    print(
        f"Binary Fuse filter size: {bf_size}, No filter size: {no_filter_size}, Difference: {bf_size - no_filter_size}"
    )
    assert (
        bf_size < no_filter_size
    ), f"Binary Fuse filter should be smaller: {bf_size} >= {no_filter_size}"

    assert_all_correct(keys, values, csf_bf)
    assert_all_correct(keys, values, csf_no_filter)


def test_get_binary_fuse_filter(tmp_path):
    keys = gen_str_keys(1000)
    values = np.array([5 for _ in range(900)] + [6] * 100)
    csf = carameldb.Caramel(
        keys, values, prefilter=BinaryFuseFilterConfig(fingerprint_bits=8)
    )

    prefilter = csf.get_filter()
    assert prefilter != None
    assert prefilter.get_most_common_value() == 5

    for i in range(900, 1000):
        assert prefilter.contains(f"key{i}")

    filename = str(tmp_path / "binary_fuse_prefilter.bin")
    prefilter.save(filename)
    assert (tmp_path / "binary_fuse_prefilter.bin").stat().st_size > 10

    loaded_filter = BinaryFusePreFilterUint32.load(filename)
    assert loaded_filter.get_most_common_value() == 5

    for i in range(900, 1000):
        assert loaded_filter.contains(f"key{i}")


def test_filter_always_created_when_config_provided():
    """Test that filter is always created when config is provided, regardless of data distribution."""
    keys = gen_str_keys(1000)
    values = np.array([5 for _ in range(len(keys))])

    csf = carameldb.Caramel(
        keys, values, prefilter=BloomFilterConfig(bits_per_element=10, num_hashes=7)
    )

    assert csf.get_filter() is not None


def test_xor_filter_fingerprint_bits_range():
    """Test that different fingerprint bit values work correctly."""
    keys = gen_str_keys(1000)
    values = np.array([5 for _ in range(800)] + [i for i in range(200)])

    for bits in [4, 8, 12, 16]:
        csf = carameldb.Caramel(
            keys, values, prefilter=XORFilterConfig(fingerprint_bits=bits)
        )
        assert_all_correct(keys, values, csf)


def test_binary_fuse_filter_fingerprint_bits_range():
    """Test that different fingerprint bit values work correctly for binary fuse."""
    keys = gen_str_keys(1000)
    values = np.array([5 for _ in range(800)] + [i for i in range(200)])

    for bits in [4, 8, 12, 16]:
        csf = carameldb.Caramel(
            keys, values, prefilter=BinaryFuseFilterConfig(fingerprint_bits=bits)
        )
        assert_all_correct(keys, values, csf)
