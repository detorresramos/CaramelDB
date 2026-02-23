"""Measure CSF file size for different filter types and alpha values."""

import os
import sys
import tempfile

import carameldb
from carameldb import BinaryFuseFilterConfig, XORFilterConfig


def gen_str_keys(n):
    return [f"key{i}" for i in range(n)]


def create_csf_with_alpha(n, alpha, filter_config):
    keys = gen_str_keys(n)

    num_most_common = int(n * alpha)
    num_other = n - num_most_common
    values = [10000] * num_most_common + list(range(num_other))

    csf = carameldb.Caramel(keys, values, prefilter=filter_config)

    with tempfile.NamedTemporaryFile(suffix=".csf", delete=False) as tmp:
        tmp_path = tmp.name

    try:
        csf.save(tmp_path)
        return os.path.getsize(tmp_path)
    finally:
        if os.path.exists(tmp_path):
            os.remove(tmp_path)


def main():
    if len(sys.argv) != 4:
        print("Usage: python measure_filter_size.py <filter_type> <n_elements> <alpha>")
        print("  filter_type: 'xor', 'binary_fuse', or 'none'")
        print("  n_elements: Number of elements (e.g., 1000, 10000)")
        print("  alpha: Normalized frequency of most common element (0.0 to 1.0)")
        sys.exit(1)

    filter_type = sys.argv[1].lower()
    n = int(sys.argv[2])
    alpha = float(sys.argv[3])

    if filter_type == "xor":
        filter_config = XORFilterConfig()
    elif filter_type == "binary_fuse":
        filter_config = BinaryFuseFilterConfig()
    elif filter_type == "none":
        filter_config = None
    else:
        print(f"Unknown filter type: {filter_type}")
        sys.exit(1)

    size_with_filter = create_csf_with_alpha(n, alpha, filter_config)
    size_without_filter = create_csf_with_alpha(n, alpha, None)

    print(f"Filter: {filter_type}, N: {n}, Alpha: {alpha:.3f}")
    print(f"Size with filter: {size_with_filter} bytes")
    print(f"Size without filter: {size_without_filter} bytes")
    print(f"Difference: {size_with_filter - size_without_filter} bytes")
    print(f"Ratio: {size_with_filter / size_without_filter:.4f}")


if __name__ == "__main__":
    main()
