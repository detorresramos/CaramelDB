import os
import sys
import tracemalloc

import numpy as np

import carameldb
from carameldb import BinaryFuseFilterConfig, BloomFilterConfig, XORFilterConfig

_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_dir, ".."))

from data_gen import compute_actual_alpha
from recommend_filter import best_binary_fuse, best_bloom, best_xor

from shibuya import empirical_entropy, shibuya_best_discrete_params


def _make_filter_config(filter_type, params):
    if filter_type == "xor":
        return XORFilterConfig(fingerprint_bits=params["fingerprint_bits"])
    elif filter_type == "binary_fuse":
        return BinaryFuseFilterConfig(fingerprint_bits=params["fingerprint_bits"])
    elif filter_type == "bloom":
        return BloomFilterConfig(
            bits_per_element=params["bloom_bits_per_element"],
            num_hashes=params["bloom_num_hashes"],
        )
    else:
        raise ValueError(f"Unknown filter type: {filter_type}")


class HashTable:
    name = "hash_table"

    @staticmethod
    def construct(keys, values):
        return {k: int(v) for k, v in zip(keys, values)}

    @staticmethod
    def query(structure, key):
        return structure[key]

    @staticmethod
    def measure_memory(keys, values):
        tracemalloc.start()
        snap_before = tracemalloc.take_snapshot()
        d = {k: int(v) for k, v in zip(keys, values)}
        snap_after = tracemalloc.take_snapshot()
        tracemalloc.stop()

        stats = snap_after.compare_to(snap_before, "lineno")
        return sum(s.size_diff for s in stats if s.size_diff > 0)

    @staticmethod
    def get_params():
        return None


EPSILON_STRATEGIES = ("optimal", "shibuya")


def _find_optimal_params(filter_type, keys, values):
    n = len(keys)
    alpha = compute_actual_alpha(values)
    n_over_N = len(np.unique(values)) / n

    if filter_type == "xor":
        bits, _ = best_xor(alpha, n_over_N)
        return {"fingerprint_bits": bits}
    elif filter_type == "binary_fuse":
        bits, _ = best_binary_fuse(alpha, n_over_N)
        return {"fingerprint_bits": bits}
    elif filter_type == "bloom":
        bpe, nh, _ = best_bloom(alpha, n_over_N)
        return {"bloom_bits_per_element": bpe, "bloom_num_hashes": nh}
    else:
        raise ValueError(f"Unknown filter type: {filter_type}")


def _find_shibuya_params(filter_type, keys, values):
    alpha = compute_actual_alpha(values)
    H0 = empirical_entropy(values)
    return shibuya_best_discrete_params(alpha, H0, filter_type)


class CSFFilter:
    """CSF + Filter with configurable epsilon strategy."""

    def __init__(self, filter_type="binary_fuse", epsilon_strategy="optimal"):
        if epsilon_strategy not in EPSILON_STRATEGIES:
            raise ValueError(
                f"Unknown epsilon_strategy: {epsilon_strategy}, "
                f"must be one of {EPSILON_STRATEGIES}"
            )
        self.filter_type = filter_type
        self.epsilon_strategy = epsilon_strategy
        self.name = f"csf_filter_{epsilon_strategy}_{filter_type}"
        self._params = None

    def construct(self, keys, values):
        if self.epsilon_strategy == "optimal":
            self._params = _find_optimal_params(self.filter_type, keys, values)
        else:
            self._params = _find_shibuya_params(self.filter_type, keys, values)
        config = _make_filter_config(self.filter_type, self._params)
        return carameldb.Caramel(keys, values, prefilter=config, verbose=False)

    @staticmethod
    def query(structure, key):
        return structure.query(key)

    @staticmethod
    def measure_memory(keys, values):
        return None

    def measure_memory_from_structure(self, structure):
        return structure.get_stats().in_memory_bytes

    def get_params(self):
        return self._params
