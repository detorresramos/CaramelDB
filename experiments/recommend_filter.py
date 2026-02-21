"""
Given a dataset with majority-value frequency alpha, compute the optimal
filter parameters to combine with a CSF.

We maximize the lower bound on bits-per-key savings (from PREFILTER_THEORY.md):

    L = δ·α·(1-ε) - n/N - b(ε)·(1-α)

where b(ε) is the bits-per-element cost of the filter and ε is its FPR.
(Assumes l₀ = 1, which holds when α > 0.5.)

For each filter type, b(ε) = C · log₂(1/ε) for a filter-specific constant C:

    Binary fuse:  C = 1.075      (from BitPackedBinaryFuseFilter.h, arity=4)
    XOR:          C = 1.23       (from BitPackedXorFilter.h)
    Bloom:        C = 1/ln2      (with optimal k = b·ln2)

Setting dL/dε = 0 gives the closed-form optimum:

    ε* = C·(1-α) / (δ·α·ln2)

For each filter type, we:
  1. Compute the closed-form ε* (theoretical optimum for continuous ε)
  2. Grid search over that filter's discrete parameters using its own b(ε)
  3. Return the parameters that maximize L
"""

import math

DELTA = 1.089  # ribbon constant for 3 hash

FILTER_OVERHEAD = {
    "binary_fuse": 1.075,
    "xor":         1.23,
    "bloom":       1.0 / math.log(2),  # ~ 1.443
}


def lower_bound(alpha, epsilon, b_eps, n_over_N):
    return DELTA * alpha * (1 - epsilon) - n_over_N - b_eps * (1 - alpha)


def fingerprint_params(filter_type, fingerprint_bits):
    C = FILTER_OVERHEAD[filter_type]
    return C * fingerprint_bits, 2 ** (-fingerprint_bits)


def bloom_params(bits_per_element, num_hashes):
    epsilon = (1 - math.exp(-num_hashes / bits_per_element)) ** num_hashes
    return bits_per_element, epsilon


def optimal_epsilon(filter_type, alpha):
    C = FILTER_OVERHEAD[filter_type]
    return C * (1 - alpha) / (DELTA * alpha * math.log(2))


def best_fingerprint_filter(filter_type, alpha, n_over_N, max_bits=8):
    best_lb, best_bits = float("-inf"), None
    for bits in range(1, max_bits + 1):
        b_eps, eps = fingerprint_params(filter_type, bits)
        lb = lower_bound(alpha, eps, b_eps, n_over_N)
        if lb > best_lb:
            best_lb, best_bits = lb, bits
    return best_bits, best_lb


def best_bloom(alpha, n_over_N, max_bpe=8, max_hashes=8):
    best_lb, best_bpe, best_nh = float("-inf"), None, None
    for bpe in range(1, max_bpe + 1):
        for nh in range(1, max_hashes + 1):
            b_eps, eps = bloom_params(bpe, nh)
            lb = lower_bound(alpha, eps, b_eps, n_over_N)
            if lb > best_lb:
                best_lb, best_bpe, best_nh = lb, bpe, nh
    return best_bpe, best_nh, best_lb


if __name__ == "__main__":
    import numpy as np
    from data_gen import gen_alpha_values

    alpha = 0.8
    N = 100_000

    values = gen_alpha_values(N, alpha, seed=42, minority_dist="unique")
    n = len(np.unique(values))
    n_over_N = n / N

    print(f"alpha = {alpha},  N = {N:,},  n = {n:,},  n/N = {n_over_N:.4f}\n")

    for filter_type in ["binary_fuse", "xor"]:
        print(f"=== {filter_type.upper()} ===")
        eps = optimal_epsilon(filter_type, alpha)
        bits, lb = best_fingerprint_filter(filter_type, alpha, n_over_N)
        print(f"  Closed-form eps* = {eps:.4f}")
        print(f"  Best discrete:  fingerprint_bits = {bits},  LB = {lb:+.4f} bits_per_key\n")

    print("=== BLOOM ===")
    eps = optimal_epsilon("bloom", alpha)
    bpe, nh, lb = best_bloom(alpha, n_over_N)
    print(f"  Closed-form eps* = {eps:.4f}")
    print(f"  Best discrete:  bits_per_element = {bpe}, num_hashes = {nh},  LB = {lb:+.4f} bits_per_key")

