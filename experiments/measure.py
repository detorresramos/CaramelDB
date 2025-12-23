"""CSF measurement and stats collection utilities."""

from dataclasses import dataclass, asdict
from typing import Optional

import carameldb
import numpy as np
from carameldb import (
    BinaryFuseFilterConfig,
    BloomFilterConfig,
    XORFilterConfig,
)

from data_gen import count_minority_keys


@dataclass
class ExperimentResult:
    """Result from a single CSF experiment."""

    # Configuration
    n: int
    alpha: float
    filter_type: str  # 'none', 'xor', 'binary_fuse', 'bloom'
    fingerprint_bits: Optional[int]
    bloom_size: Optional[int]
    bloom_num_hashes: Optional[int]
    minority_dist: str  # 'unique', 'zipfian', 'uniform_10', 'uniform_100', 'two_value'

    # Size metrics (bytes)
    total_bytes: int
    solution_bytes: float
    filter_bytes: float
    metadata_bytes: float

    # Derived metrics
    bits_per_key: float

    # Filter stats
    num_filter_elements: Optional[int]

    # Huffman stats
    huffman_num_symbols: int
    huffman_avg_bits: float

    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization."""
        return asdict(self)

    @classmethod
    def from_dict(cls, d: dict) -> "ExperimentResult":
        """Create from dictionary."""
        return cls(**d)


def create_filter_config(
    filter_type: str,
    n: int,
    alpha: float,
    fingerprint_bits: Optional[int] = None,
    bloom_bits_per_element: Optional[int] = None,
    bloom_num_hashes: Optional[int] = None,
):
    """
    Create filter configuration.

    For Bloom filters, the size is computed as:
        size = bloom_bits_per_element * num_minority_keys
    where num_minority_keys = n * (1 - alpha)

    Args:
        filter_type: 'none', 'xor', 'binary_fuse', or 'bloom'
        n: Number of keys
        alpha: Frequency of most common element
        fingerprint_bits: For XOR/BinaryFuse filters
        bloom_bits_per_element: Bits per element for Bloom filter
        bloom_num_hashes: Number of hash functions for Bloom filter

    Returns:
        Filter config object or None
    """
    if filter_type == "none":
        return None
    elif filter_type == "xor":
        if fingerprint_bits is None:
            raise ValueError("fingerprint_bits required for xor filter")
        return XORFilterConfig(fingerprint_bits=fingerprint_bits)
    elif filter_type == "binary_fuse":
        if fingerprint_bits is None:
            raise ValueError("fingerprint_bits required for binary_fuse filter")
        return BinaryFuseFilterConfig(fingerprint_bits=fingerprint_bits)
    elif filter_type == "bloom":
        if bloom_bits_per_element is None or bloom_num_hashes is None:
            raise ValueError(
                "bloom_bits_per_element and bloom_num_hashes required for bloom filter"
            )
        num_minority = int(n * (1 - alpha))
        size = max(bloom_bits_per_element * num_minority, 64)
        return BloomFilterConfig(size=size, num_hashes=bloom_num_hashes)
    else:
        raise ValueError(f"Unknown filter type: {filter_type}")


def measure_csf(
    keys: list,
    values: np.ndarray,
    filter_type: str,
    n: int,
    alpha: float,
    fingerprint_bits: Optional[int] = None,
    bloom_bits_per_element: Optional[int] = None,
    bloom_num_hashes: Optional[int] = None,
    minority_dist: str = "unique",
) -> ExperimentResult:
    """
    Build CSF and collect all stats.

    Args:
        keys: List of string keys
        values: Array of values
        filter_type: 'none', 'xor', 'binary_fuse', or 'bloom'
        n: Number of keys
        alpha: Target alpha value
        fingerprint_bits: For XOR/BinaryFuse filters
        bloom_bits_per_element: Bits per element for Bloom filter
        bloom_num_hashes: Number of hash functions for Bloom filter
        minority_dist: Distribution of minority values

    Returns:
        ExperimentResult with all measurements
    """
    filter_config = create_filter_config(
        filter_type=filter_type,
        n=n,
        alpha=alpha,
        fingerprint_bits=fingerprint_bits,
        bloom_bits_per_element=bloom_bits_per_element,
        bloom_num_hashes=bloom_num_hashes,
    )

    csf = carameldb.Caramel(keys, values, prefilter=filter_config, verbose=False)
    stats = csf.get_stats()

    # Extract filter-specific stats
    num_filter_elements = None
    actual_bloom_size = None
    actual_bloom_num_hashes = None
    actual_fingerprint_bits = fingerprint_bits

    if stats.filter_stats is not None:
        num_filter_elements = stats.filter_stats.num_elements
        if stats.filter_stats.type == "bloom":
            actual_bloom_size = stats.filter_stats.size_bits
            actual_bloom_num_hashes = stats.filter_stats.num_hashes
        elif stats.filter_stats.type in ("xor", "binary_fuse"):
            actual_fingerprint_bits = stats.filter_stats.fingerprint_bits

    return ExperimentResult(
        n=n,
        alpha=alpha,
        filter_type=filter_type,
        fingerprint_bits=actual_fingerprint_bits,
        bloom_size=actual_bloom_size,
        bloom_num_hashes=actual_bloom_num_hashes,
        minority_dist=minority_dist,
        total_bytes=stats.in_memory_bytes,
        solution_bytes=stats.solution_bytes,
        filter_bytes=stats.filter_bytes,
        metadata_bytes=stats.metadata_bytes,
        bits_per_key=(stats.in_memory_bytes * 8) / n,
        num_filter_elements=num_filter_elements,
        huffman_num_symbols=stats.huffman_stats.num_unique_symbols,
        huffman_avg_bits=stats.huffman_stats.avg_bits_per_symbol,
    )


def get_filter_config_str(result: ExperimentResult) -> str:
    """
    Get a string representation of the filter configuration.

    Args:
        result: ExperimentResult

    Returns:
        String like "none", "xor_8", "binary_fuse_12", "bloom_10_7"
    """
    if result.filter_type == "none":
        return "none"
    elif result.filter_type in ("xor", "binary_fuse"):
        return f"{result.filter_type}_{result.fingerprint_bits}"
    elif result.filter_type == "bloom":
        return f"bloom_{result.bloom_size}_{result.bloom_num_hashes}"
    else:
        return result.filter_type
