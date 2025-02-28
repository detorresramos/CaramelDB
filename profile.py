from time import perf_counter_ns

import numpy as np

import carameldb


def benchmark_dict(keys, query_keys, num_queries):
    # Build dictionary
    d = {k.decode("utf-8"): v for k, v in zip(keys, range(len(keys)))}

    # Warm up
    for _ in range(100):
        d[query_keys[0].decode("utf-8")]

    # Measure query time
    total_time = 0
    print(f"Running {num_queries} dictionary queries...")

    for key in query_keys:
        start = perf_counter_ns()
        d[key.decode("utf-8")]
        total_time += perf_counter_ns() - start

    return total_time / num_queries


def benchmark_csf(N, num_queries=10000):
    # Create keys and values
    keys = [str(i).encode("utf-8") for i in range(N)]
    values = list(range(N))

    # Build the CSF
    print(f"\nBuilding CSF with {N} elements...")
    csf = carameldb.PyCsfUint32.construct(keys, values)

    # Generate random query keys that exist in the dataset
    query_keys = [
        str(np.random.randint(0, N)).encode("utf-8") for _ in range(num_queries)
    ]

    # Warm up
    for _ in range(100):
        csf.query(query_keys[0])

    # Measure query time
    total_time = 0
    print(f"Running {num_queries} CSF queries...")

    for key in query_keys:
        start = perf_counter_ns()
        csf.query(key)
        total_time += perf_counter_ns() - start

    csf_avg_time = total_time / num_queries

    # Benchmark dictionary with same keys
    dict_avg_time = benchmark_dict(keys, query_keys, num_queries)

    print(f"\nResults for N={N}:")
    print(f"CSF average query time: {csf_avg_time:.2f} ns")
    print(f"Dict average query time: {dict_avg_time:.2f} ns")
    print(f"CSF is {dict_avg_time/csf_avg_time:.2f}x faster than dict")

    return csf_avg_time, dict_avg_time


def main():
    sizes = [10000, 100000, 1000000]
    results = {}

    for N in sizes:
        results[N] = benchmark_csf(N)

    print("\nSummary:")
    print("-" * 50)
    for N, (csf_time, dict_time) in results.items():
        print(f"N={N:,}:")
        print(f"  CSF: {csf_time:.2f} ns per query")
        print(f"  Dict: {dict_time:.2f} ns per query")
        print(f"  Speed ratio: {dict_time/csf_time:.2f}x")
        print()


if __name__ == "__main__":
    main()
