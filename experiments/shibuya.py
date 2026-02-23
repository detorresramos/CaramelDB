"""Shibuya's cost model for CSF+Filter epsilon derivation.

Implements the alternative epsilon selection from Shibuya's analysis:
    epsilon* = C_BF * (1 - alpha) / (C_CSF * alpha * ln2)

where C_BF = 1.44 (idealized Bloom constant) and C_CSF is derived from
empirical entropy of the value distribution.
"""

import math

import numpy as np
from theory import binary_fuse_params, bloom_params, xor_params

C_BF = 1.44


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
    return 1.1 * H0 + 0.2


def shibuya_optimal_epsilon(alpha, H0):
    """epsilon* = C_BF * (1 - alpha) / (C_CSF * alpha * ln2)"""
    C_CSF = shibuya_csf_cost(H0)
    return C_BF * (1 - alpha) / (C_CSF * alpha * math.log(2))


def _closest_discrete_params(eps_star, candidates):
    """Find the candidate whose epsilon is closest to eps_star in log space."""
    best_candidate = None
    best_dist = float("inf")
    for candidate in candidates:
        dist = abs(math.log(candidate["eps"]) - math.log(max(eps_star, 1e-15)))
        if dist < best_dist:
            best_dist = dist
            best_candidate = candidate
    return best_candidate


def shibuya_best_discrete_params(alpha, H0, filter_type):
    """Map Shibuya's optimal epsilon to the nearest discrete filter parameters.

    Returns dict with filter parameters and the achieved epsilon.
    """
    eps_star = shibuya_optimal_epsilon(alpha, H0)

    if filter_type in ("xor", "binary_fuse"):
        param_fn = xor_params if filter_type == "xor" else binary_fuse_params
        candidates = [{"bits": bits, "eps": param_fn(bits)[1]} for bits in range(1, 9)]
        best = _closest_discrete_params(eps_star, candidates)
        _, achieved_eps = param_fn(best["bits"])
        return {
            "fingerprint_bits": best["bits"],
            "epsilon": achieved_eps,
            "epsilon_star": eps_star,
        }

    elif filter_type == "bloom":
        candidates = [
            {"bpe": bpe, "k": k, "eps": bloom_params(bpe, k)[1]}
            for bpe in range(1, 9)
            for k in range(1, 9)
        ]
        best = _closest_discrete_params(eps_star, candidates)
        _, achieved_eps = bloom_params(best["bpe"], best["k"])
        return {
            "bloom_bits_per_element": best["bpe"],
            "bloom_num_hashes": best["k"],
            "epsilon": achieved_eps,
            "epsilon_star": eps_star,
        }

    else:
        raise ValueError(f"Unknown filter type: {filter_type}")
