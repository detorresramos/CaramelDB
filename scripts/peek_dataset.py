"""Peek at the first few rows of a multiset dataset .npy file.

Usage:
    python scripts/peek_dataset.py <path>               # first 10 rows
    python scripts/peek_dataset.py <path> -n 5          # first 5 rows
    python scripts/peek_dataset.py <path> -n 3 -c 8     # first 3 rows, first 8 cols

Example:
    python scripts/peek_dataset.py datasets/query_100m_m10.npy
"""

import argparse

import numpy as np


def main():
    p = argparse.ArgumentParser()
    p.add_argument("path", help="Path to the .npy dataset")
    p.add_argument("-n", "--rows", type=int, default=10, help="Rows to show (default 10)")
    p.add_argument("-c", "--cols", type=int, default=None,
                   help="Columns to show per row (default: all)")
    args = p.parse_args()

    a = np.load(args.path, mmap_mode="r")
    print(f"file:  {args.path}")
    print(f"shape: {a.shape}  dtype: {a.dtype}  size: {a.nbytes / 1e9:.2f} GB")
    print()

    view = a[: args.rows]
    if args.cols is not None:
        view = view[:, : args.cols]

    width = max(len(str(int(view.max()))), 6)
    header = "  row | " + "  ".join(f"c{j:<{width - 1}}" for j in range(view.shape[1]))
    print(header)
    print("  " + "-" * (len(header) - 2))
    for i, row in enumerate(view):
        cells = "  ".join(f"{int(v):>{width}}" for v in row)
        print(f"  {i:>3} | {cells}")


if __name__ == "__main__":
    main()
