import argparse
import os
import time

import carameldb
import numpy as np


class dotdict(dict):
    """dot.notation access to dictionary attributes"""

    __getattr__ = dict.get
    __setattr__ = dict.__setitem__
    __delattr__ = dict.__delitem__


def parse_args():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )

    parser.add_argument(
        "--dist",
        type=str,
        choices=["zipfian", "geometric", "uniform"],
        default="zipfian",
    )
    parser.add_argument("--rows", type=int, default=10_000_000)
    parser.add_argument("--columns", type=int, default=1)
    parser.add_argument("--zipfian_s_val", type=int, default=1.6)
    parser.add_argument("--uniform_num_unique_values", type=int, default=64)
    parser.add_argument("--geometric_p_val", type=float, default=0.5)
    parser.add_argument("--parallel_queries", action="store_true")

    return dotdict(vars(parser.parse_args()))


def get_values(distribution_name, rows, columns, args):
    all_values = []

    for _ in range(columns):
        if distribution_name == "uniform":
            all_values.append(
                np.random.randint(args.uniform_num_unique_values, size=rows)
            )
        elif distribution_name == "zipfian":
            all_values.append(np.random.zipf(args.zipfian_s_val, size=rows))
        elif distribution_name == "geometric":
            all_values.append(np.random.geometric(args.geometric_p_val, size=rows))
        else:
            raise ValueError(f"Invalid distribution provided: {distribution_name}")

    if columns == 1:
        return all_values[0]

    return np.array(all_values).T


def main(args):
    keys = np.array([f"key{i}" for i in range(args.rows)])
    values = get_values(
        distribution_name=args.dist, rows=args.rows, columns=args.columns, args=args
    ).astype(np.uint32)

    start = time.time()
    csf = carameldb.Caramel(keys, values, verbose=False)
    construction_time = time.time() - start

    filename = "test_file.csf"
    csf.save(filename)
    csf_size = os.path.getsize(filename)
    csf = carameldb.load(filename)

    os.remove(filename)

    csf_query_time = 0
    for i, key in enumerate(keys):
        start = time.perf_counter_ns()
        if args.columns != 1:
            val = csf.query(key, parallel=args.parallel_queries)
        else:
            val = csf.query(key)
        csf_query_time += time.perf_counter_ns() - start
        if args.columns == 1:
            assert val == values[i]
        else:
            assert list(val) == list(values[i])
    csf_query_time /= len(keys)

    lookup = {}
    for key, value in zip(keys, values):
        lookup[key] = value

    print(
        f"Results for distribution '{args.dist}' with {args.rows} rows and {args.columns} columns.\n"
    )
    print(
        f"Construction Time: {construction_time:.3f} seconds or {(construction_time * 1e6) / args.rows:.3f} microseconds / key"
    )
    print(f"File Size: {csf_size} bytes or {csf_size / args.rows:.3f} bytes / key")
    print(f"Average Query Time: {csf_query_time:.3f} ns")


if __name__ == "__main__":
    args = parse_args()

    main(args)
