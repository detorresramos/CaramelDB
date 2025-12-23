import carameldb
import numpy as np
import pytest
from carameldb import (
    BinaryFuseFilterConfig,
    BloomFilterConfig,
    XORFilterConfig,
)


def gen_str_keys(n):
    return [f"key{i}" for i in range(n)]


def test_csf_stats_basic():
    """Test basic CSF stats without filter."""
    keys = gen_str_keys(1000)
    values = np.array([i % 10 for i in range(1000)], dtype=np.uint32)

    csf = carameldb.Caramel(keys, values, verbose=False)
    stats = csf.get_stats()

    # Basic sanity checks
    assert stats.in_memory_bytes > 0
    assert stats.bucket_stats.num_buckets > 0
    assert stats.huffman_stats.num_unique_symbols == 10
    assert stats.huffman_stats.max_code_length > 0
    assert stats.huffman_stats.avg_bits_per_symbol > 0
    assert stats.filter_stats is None


def test_csf_stats_with_bloom_filter():
    """Test CSF stats with Bloom filter."""
    keys = gen_str_keys(1000)
    values = np.array([0] * 800 + list(range(200)), dtype=np.uint32)

    csf = carameldb.Caramel(
        keys, values,
        prefilter=BloomFilterConfig(bits_per_element=10, num_hashes=7),
        verbose=False
    )
    stats = csf.get_stats()

    # Check filter stats
    assert stats.filter_stats is not None
    assert stats.filter_stats.type == "bloom"
    assert stats.filter_stats.size_bytes > 0
    assert stats.filter_stats.num_hashes == 7
    # 199 minority elements * 10 bits = 1990 bits
    assert stats.filter_stats.size_bits == 1990

    # Check memory breakdown
    assert stats.solution_bytes > 0
    assert stats.filter_bytes > 0
    assert stats.metadata_bytes > 0


def test_csf_stats_with_xor_filter():
    """Test CSF stats with XOR filter."""
    keys = gen_str_keys(1000)
    values = np.array([0] * 800 + list(range(200)), dtype=np.uint32)

    csf = carameldb.Caramel(
        keys, values,
        prefilter=XORFilterConfig(fingerprint_bits=8),
        verbose=False
    )
    stats = csf.get_stats()

    # Check filter stats
    assert stats.filter_stats is not None
    assert stats.filter_stats.type == "xor"
    assert stats.filter_stats.size_bytes > 0
    assert stats.filter_stats.fingerprint_bits == 8
    # 199 elements because range(200) includes 0 which is also the most common value
    assert stats.filter_stats.num_elements == 199


def test_csf_stats_with_binary_fuse_filter():
    """Test CSF stats with Binary Fuse filter."""
    keys = gen_str_keys(1000)
    values = np.array([0] * 800 + list(range(200)), dtype=np.uint32)

    csf = carameldb.Caramel(
        keys, values,
        prefilter=BinaryFuseFilterConfig(fingerprint_bits=12),
        verbose=False
    )
    stats = csf.get_stats()

    # Check filter stats
    assert stats.filter_stats is not None
    assert stats.filter_stats.type == "binary_fuse"
    assert stats.filter_stats.size_bytes > 0
    assert stats.filter_stats.fingerprint_bits == 12
    # 199 elements because range(200) includes 0 which is also the most common value
    assert stats.filter_stats.num_elements == 199


def test_csf_stats_huffman_distribution():
    """Test that Huffman statistics are computed correctly."""
    keys = gen_str_keys(1000)
    # Create values with varying frequencies for interesting Huffman codes
    values = np.array(
        [0] * 500 + [1] * 250 + [2] * 125 + [3] * 62 + [4] * 63,
        dtype=np.uint32
    )

    csf = carameldb.Caramel(keys, values, verbose=False)
    stats = csf.get_stats()

    # Check Huffman stats
    assert stats.huffman_stats.num_unique_symbols == 5
    assert len(stats.huffman_stats.code_length_distribution) > 0
    # Total count in distribution should equal number of unique symbols
    total = sum(stats.huffman_stats.code_length_distribution)
    assert total == 5


def test_csf_stats_bucket_stats():
    """Test that bucket statistics are computed correctly."""
    keys = gen_str_keys(5000)  # Enough for multiple buckets
    values = np.array([i % 100 for i in range(5000)], dtype=np.uint32)

    csf = carameldb.Caramel(keys, values, verbose=False)
    stats = csf.get_stats()

    # Check bucket stats
    assert stats.bucket_stats.num_buckets >= 1
    assert stats.bucket_stats.total_solution_bits > 0
    assert stats.bucket_stats.avg_solution_bits > 0
    assert stats.bucket_stats.min_solution_bits > 0
    assert stats.bucket_stats.max_solution_bits >= stats.bucket_stats.min_solution_bits


def test_csf_stats_memory_breakdown():
    """Test that memory breakdown adds up correctly."""
    keys = gen_str_keys(1000)
    values = np.array([0] * 800 + list(range(200)), dtype=np.uint32)

    csf = carameldb.Caramel(
        keys, values,
        prefilter=BloomFilterConfig(bits_per_element=10, num_hashes=7),
        verbose=False
    )
    stats = csf.get_stats()

    # Memory breakdown should approximately equal total
    breakdown_sum = stats.solution_bytes + stats.filter_bytes + stats.metadata_bytes
    assert abs(stats.in_memory_bytes - breakdown_sum) < 100  # Allow small rounding difference
