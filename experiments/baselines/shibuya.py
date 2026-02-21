"""Shibuya's cost model for CSF+Filter epsilon derivation.

Implements the alternative epsilon selection from Shibuya's analysis:
    epsilon* = C_BF * (1 - alpha) / (C_CSF * alpha * ln2)

where C_BF = 1.44 (idealized Bloom constant) and C_CSF is derived from
empirical entropy of the value distribution.
"""

import math
import os
import sys

import numpy as np

_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_dir, ".."))

from recommend_filter import (
    best_binary_fuse,
    best_bloom,
    best_xor,
    binary_fuse_params,
    bloom_params,
    lower_bound,
    xor_params,
)

C_BF = 1.44  # idealized Bloom filter constant


def empirical_entropy(values):
    """Compute H0 (zeroth-order empirical entropy) from value distribution."""
    _, counts = np.unique(values, return_counts=True)
    probs = counts / counts.sum()
    return -np.sum(probs * np.log2(probs))


def shibuya_csf_cost(H0):
    """Piecewise C_CSF cost function from Shibuya's model.

    For low-entropy distributions (H0 < 2), the CSF cost per symbol is
    quadratic in H0. For higher entropy, it's approximately linear.
    """
    if H0 < 2:
        return 0.22 * H0**2 + 0.18 * H0 + 1.16
    else:
        return 1.1 * H0 + 0.2


def shibuya_optimal_epsilon(alpha, H0):
    """Compute Shibuya's optimal FPR.

    epsilon* = C_BF * (1 - alpha) / (C_CSF * alpha * ln2)
    """
    C_CSF = shibuya_csf_cost(H0)
    return C_BF * (1 - alpha) / (C_CSF * alpha * math.log(2))


def shibuya_best_discrete_params(alpha, H0, filter_type, n_over_N=0.0):
    """Map Shibuya's optimal epsilon to the nearest discrete filter parameters.

    For xor/binary_fuse: finds the fingerprint_bits whose epsilon is closest
    to Shibuya's epsilon*.
    For bloom: finds the (bpe, k) combination whose epsilon is closest.

    Returns:
        dict with filter parameters and the achieved epsilon.
    """
    eps_star = shibuya_optimal_epsilon(alpha, H0)

    if filter_type in ("xor", "binary_fuse"):
        param_fn = xor_params if filter_type == "xor" else binary_fuse_params
        best_bits = 1
        best_dist = float("inf")
        for bits in range(1, 9):
            _, eps = param_fn(bits)
            dist = abs(math.log(eps) - math.log(max(eps_star, 1e-15)))
            if dist < best_dist:
                best_dist = dist
                best_bits = bits

        _, achieved_eps = param_fn(best_bits)
        return {
            "fingerprint_bits": best_bits,
            "epsilon": achieved_eps,
            "epsilon_star": eps_star,
        }

    elif filter_type == "bloom":
        best_bpe, best_k = 1, 1
        best_dist = float("inf")
        for bpe in range(1, 9):
            for k in range(1, 9):
                _, eps = bloom_params(bpe, k)
                dist = abs(math.log(eps) - math.log(max(eps_star, 1e-15)))
                if dist < best_dist:
                    best_dist = dist
                    best_bpe, best_k = bpe, k

        _, achieved_eps = bloom_params(best_bpe, best_k)
        return {
            "bloom_bits_per_element": best_bpe,
            "bloom_num_hashes": best_k,
            "epsilon": achieved_eps,
            "epsilon_star": eps_star,
        }

    else:
        raise ValueError(f"Unknown filter type: {filter_type}")
