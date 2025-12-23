#!/usr/bin/env python3
"""
Debug script to investigate Binary Fuse filter behavior.
Tests filter creation with different parameters and logs verbose output.
"""

import os
import tempfile
import carameldb
from carameldb import BinaryFuseFilterConfig, XORFilterConfig, BloomFilterConfig

def test_filter_with_params(filter_type, n, alpha):
    """Test a filter with specific parameters and report details."""
    print(f"\n{'='*70}")
    print(f"Testing {filter_type} Filter: N={n:,}, α={alpha:.2f}")
    print(f"{'='*70}")

    # Generate test data (matching quick_alpha_scan.py)
    keys = [f"key{i}" for i in range(n)]
    num_most_common = int(n * alpha)
    num_other = n - num_most_common
    values = [10000 for _ in range(num_most_common)] + [i for i in range(num_other)]

    # Create filter config
    if filter_type == "binary_fuse":
        filter_config = BinaryFuseFilterConfig(fingerprint_bits=8)
    elif filter_type == "xor":
        filter_config = XORFilterConfig(fingerprint_bits=8)
    elif filter_type == "bloom":
        filter_config = BloomFilterConfig(bits_per_element=10, num_hashes=7)
    else:
        raise ValueError(f"Unknown filter type: {filter_type}")

    # Create CSF with verbose output
    print(f"\nCreating CSF with {filter_type} filter (verbose=True)...")
    csf = carameldb.Caramel(keys, values, prefilter=filter_config, verbose=True)

    # Get total size
    with tempfile.NamedTemporaryFile(delete=False, suffix='.csf') as tmp:
        tmp_path = tmp.name

    try:
        csf.save(tmp_path)
        total_size = os.path.getsize(tmp_path)
        print(f"\n[RESULT] Total CSF size: {total_size:,} bytes")
    finally:
        if os.path.exists(tmp_path):
            os.remove(tmp_path)

    return csf

# Test at high alpha (where Binary Fuse should excel)
print("\n" + "="*70)
print("DIAGNOSTIC: Testing filters at α=0.95, N=1,000,000")
print("="*70)

test_filter_with_params("binary_fuse", 1_000_000, 0.95)
print("\n" + "-"*70)
test_filter_with_params("xor", 1_000_000, 0.95)

# Test at medium alpha
print("\n\n" + "="*70)
print("DIAGNOSTIC: Testing filters at α=0.90, N=1,000,000")
print("="*70)

test_filter_with_params("binary_fuse", 1_000_000, 0.90)
print("\n" + "-"*70)
test_filter_with_params("xor", 1_000_000, 0.90)

# Test smaller dataset
print("\n\n" + "="*70)
print("DIAGNOSTIC: Testing filters at α=0.95, N=100,000")
print("="*70)

test_filter_with_params("binary_fuse", 100_000, 0.95)
print("\n" + "-"*70)
test_filter_with_params("xor", 100_000, 0.95)
