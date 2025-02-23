import time
import numpy as np
from statistics import mean, stdev
import random
import string

def generate_random_string(length=10):
    """Generate a random string of fixed length"""
    return ''.join(random.choices(string.ascii_letters + string.digits, k=length))

def profile_bloom_filter(size, error_rate=0.05, num_queries=10, num_trials=3):
    """Profile BloomFilter operations for a given size"""
    from carameldb import PyBloomFilter
    
    # Initialize timing results
    construction_times = []
    query_times_hit = []
    query_times_miss = []
    
    for trial in range(num_trials):
        # Generate random strings for insertion
        strings_to_insert = [generate_random_string() for _ in range(size)]
        
        # Measure construction time
        start_time = time.perf_counter()
        bloom = PyBloomFilter(size, error_rate)
        for s in strings_to_insert:
            bloom.add(s)
        construction_times.append(time.perf_counter() - start_time)
        
        # Generate strings for querying (half existing, half new)
        query_strings_hit = random.sample(strings_to_insert, num_queries)
        query_strings_miss = [generate_random_string() for _ in range(num_queries)]
        
        # Measure query time for hits
        start_time = time.perf_counter()
        for s in query_strings_hit:
            _ = bloom.contains(s)
        query_times_hit.append((time.perf_counter() - start_time) / num_queries)
        
        # Measure query time for misses
        start_time = time.perf_counter()
        for s in query_strings_miss:
            _ = bloom.contains(s)
        query_times_miss.append((time.perf_counter() - start_time) / num_queries)
    
    return {
        'size': size,
        'construction_time': {
            'mean': mean(construction_times),
            'std': stdev(construction_times) if len(construction_times) > 1 else 0
        },
        'query_time_hit': {
            'mean': mean(query_times_hit),
            'std': stdev(query_times_hit) if len(query_times_hit) > 1 else 0
        },
        'query_time_miss': {
            'mean': mean(query_times_miss),
            'std': stdev(query_times_miss) if len(query_times_miss) > 1 else 0
        }
    }

def main():
    sizes = [1000, 10000, 100000]
    error_rate = 0.05
    num_queries = 100
    num_trials = 3
    
    print(f"\nProfiling BloomFilter with {num_trials} trials, {num_queries} queries per trial")
    print(f"Error rate: {error_rate}")
    print("-" * 80)
    
    for size in sizes:
        results = profile_bloom_filter(size, error_rate, num_queries, num_trials)
        
        print(f"\nSize: {size}")
        print(f"Construction time: {results['construction_time']['mean']*1000:.2f} ms ± {results['construction_time']['std']*1000:.2f} ms")
        print(f"Query time (hits): {results['query_time_hit']['mean']*1e6:.2f} µs ± {results['query_time_hit']['std']*1e6:.2f} µs")
        print(f"Query time (misses): {results['query_time_miss']['mean']*1e6:.2f} µs ± {results['query_time_miss']['std']*1e6:.2f} µs")

if __name__ == "__main__":
    main()