import argparse
import os
import time

import caramel
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
        "--distribution",
        type=str,
        choices=["zipfian", "geometric", "uniform"],
    )
    parser.add_argument("--size", type=int)
    parser.add_argument("--zipfian_s_val", type=int, default=2)
    parser.add_argument("--uniform_num_unique_values", type=int, default=64)
    parser.add_argument("--geometric_p_val", type=float, default=0.5)

    return dotdict(vars(parser.parse_args()))


def get_values(distribution_name, size, args):
    if distribution_name == "uniform":
        return np.random.randint(args.uniform_num_unique_values, size=size)

    if distribution_name == "zipfian":
        return np.random.zipf(args.zipfian_s_val, size=size)

    if distribution_name == "geometric":
        return np.random.geometric(args.geometric_p_val, size=size)

    raise ValueError(f"Invalid distribution provided: {distribution_name}")


def main(args):
    keys = [f"key{i}" for i in range(args.size)]
    values = get_values(distribution_name=args.distribution, size=args.size, args=args)

    start = time.time()
    csf = caramel.CSF(keys, values)
    construction_time = time.time() - start

    filename = "test_file.csf"
    csf.save(filename)
    csf_size = os.path.getsize(filename)
    csf = caramel.load(filename)
    os.remove(filename)

    query_time = 0
    for i, key in enumerate(keys):
        start = time.perf_counter_ns()
        val = csf.query(key)
        query_time += time.perf_counter_ns() - start
        assert val == values[i]
    query_time /= len(keys)

    print(f"Results for distribution '{args.distribution}' and size '{args.size}'")
    print(
        f"Construction Time: {construction_time:.3f} seconds or {(construction_time * 1e6) / args.size:.3f} microseconds / key"
    )
    print(f"File Size: {csf_size} bytes or {csf_size / args.size:.3f} bytes / key")
    print(f"Average Query Time: {query_time:.3f} ns")


if __name__ == "__main__":
    args = parse_args()

    main(args)
