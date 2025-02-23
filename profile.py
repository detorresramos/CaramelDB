from time import perf_counter_ns
import carameldb
import numpy as np

def benchmark_csf(N, num_queries=10000):
    # Create keys and values
    keys = [str(i).encode('utf-8') for i in range(N)]
    values = list(range(N))
    
    # Build the CSF
    print(f"\nBuilding CSF with {N} elements...")
    csf = carameldb.PyCsfUint32.construct(keys, values)
    
    # Generate random query keys that exist in the dataset
    query_keys = [str(np.random.randint(0, N)).encode('utf-8') for _ in range(num_queries)]
    
    # Warm up
    for _ in range(100):
        csf.query(query_keys[0])
    
    # Measure query time
    total_time = 0
    print(f"Running {num_queries} queries...")
    
    for key in query_keys:
        start = perf_counter_ns()
        csf.query(key)
        total_time += perf_counter_ns() - start
    
    avg_time = total_time / num_queries
    print(f"Average query time for N={N}: {avg_time:.2f} ns")
    return avg_time

def main():
    sizes = [10000, 100000, 1000000]
    results = {}
    
    for N in sizes:
        results[N] = benchmark_csf(N)
    
    print("\nSummary:")
    print("-" * 50)
    for N, avg_time in results.items():
        print(f"N={N:,}: {avg_time:.2f} ns per query")

if __name__ == "__main__":
    main()
