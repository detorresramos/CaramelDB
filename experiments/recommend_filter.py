"""
Given a dataset with majority-value frequency alpha, compute the optimal
filter parameters to combine with a CSF.

We maximize the lower bound on bits-per-key savings (from PREFILTER_THEORY.md):

    L = delta * alpha * (1-eps) - n/N - b(eps) * (1-alpha)

where b(eps) is the bits-per-element cost of the filter and eps is its FPR.
(Assumes l_0 = 1, which holds when alpha > 0.5.)

For each filter type, b(eps) = C * log2(1/eps) for a filter-specific constant C:

    Binary fuse:  C = 1.075      (from BitPackedBinaryFuseFilter.h, arity=4)
    XOR:          C = 1.23       (from BitPackedXorFilter.h)
    Bloom:        C = 1/ln2      (with optimal k = b*ln2)

Setting dL/deps = 0 gives the closed-form optimum:

    eps* = C*(1-alpha) / (delta*alpha*ln2)

For each filter type, we:
  1. Compute the closed-form eps* (theoretical optimum for continuous eps)
  2. Grid search over that filter's discrete parameters using its own b(eps)
  3. Return the parameters that maximize L
"""

from theory import (
    best_discrete_binary_fuse,
    best_discrete_bloom,
    best_discrete_xor,
    bloom_params,
    optimal_epsilon_binary_fuse,
    optimal_epsilon_xor,
)

OPTIMAL_EPSILON_FNS = {
    "binary_fuse": optimal_epsilon_binary_fuse,
    "xor": optimal_epsilon_xor,
}

BEST_DISCRETE_FNS = {
    "binary_fuse": best_discrete_binary_fuse,
    "xor": best_discrete_xor,
}


def best_bloom(alpha, n_over_N, max_bpe=8, max_hashes=8):
    best_lb, best_bpe, best_nh = float("-inf"), None, None
    for nh in range(1, max_hashes + 1):
        bpe, lb = best_discrete_bloom(alpha, n_over_N, nh, max_bpe)
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
        eps = OPTIMAL_EPSILON_FNS[filter_type](alpha)
        bits, lb = BEST_DISCRETE_FNS[filter_type](alpha, n_over_N)
        print(f"  Closed-form eps* = {eps:.4f}")
        print(
            f"  Best discrete:  fingerprint_bits = {bits},  LB = {lb:+.4f} bits_per_key\n"
        )

    print("=== BLOOM ===")
    bpe, nh, lb = best_bloom(alpha, n_over_N)
    _, eps = bloom_params(bpe, nh)
    print(
        f"  Best discrete:  bits_per_element = {bpe}, num_hashes = {nh},  LB = {lb:+.4f} bits_per_key"
    )
