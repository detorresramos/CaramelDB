"""Shared theory: bounds, b(epsilon), optimal epsilon for all filter types."""

import math

DELTA = 1.089
XOR_C = 1.23
BINARY_FUSE_C_ASYMPTOTIC = 1.075


def binary_fuse_C(n_filter):
    """Size-dependent overhead constant for binary fuse filters.

    From BitPackedBinaryFuseFilter.h calculateSizeFactor (arity=4).
    Approaches 1.075 for large n, but significantly higher for small filters.
    """
    if n_filter is None or n_filter <= 1:
        return BINARY_FUSE_C_ASYMPTOTIC
    return max(1.075, 0.77 + 0.305 * math.log(600000) / math.log(n_filter))


# ---------------------------------------------------------------------------
# b(epsilon) per filter type
# ---------------------------------------------------------------------------

def b_eps_xor(eps):
    return XOR_C * math.log2(1 / eps)


def b_eps_binary_fuse(eps, n_filter=None):
    C = binary_fuse_C(n_filter)
    return C * math.log2(1 / eps)


def b_eps_bloom(eps, k):
    if eps <= 0 or eps >= 1:
        return float("inf")
    root = eps ** (1 / k)
    if root >= 1:
        return float("inf")
    return -k / math.log(1 - root)


# ---------------------------------------------------------------------------
# Discrete param -> (b_eps, epsilon)
# ---------------------------------------------------------------------------

def xor_params(fingerprint_bits):
    return XOR_C * fingerprint_bits, 2 ** (-fingerprint_bits)


def binary_fuse_params(fingerprint_bits, n_filter=None):
    C = binary_fuse_C(n_filter)
    return C * fingerprint_bits, 2 ** (-fingerprint_bits)


def bloom_params(bpe, k):
    eps = (1 - math.exp(-k / bpe)) ** k
    return bpe, eps


# ---------------------------------------------------------------------------
# Binary entropy
# ---------------------------------------------------------------------------

def binary_entropy(alpha):
    if alpha <= 0 or alpha >= 1:
        return 0.0
    return -alpha * math.log2(alpha) - (1 - alpha) * math.log2(1 - alpha)


# ---------------------------------------------------------------------------
# Bounds (same formula, different b(epsilon))
# ---------------------------------------------------------------------------

def lower_bound(alpha, eps, b_eps, n_over_N):
    """L = delta * alpha * (1-eps) - n/N - b(eps) * (1-alpha)"""
    return DELTA * alpha * (1 - eps) - n_over_N - b_eps * (1 - alpha)


def upper_bound(alpha, eps, b_eps, n_over_N):
    """U = delta * (1 + H(alpha) - 2*alpha*eps*(1-alpha)/(3-alpha)) + n/N - b(eps)*(1-alpha)"""
    return (
        DELTA * (1 + binary_entropy(alpha) - 2 * alpha * eps * (1 - alpha) / (3 - alpha))
        + n_over_N
        - b_eps * (1 - alpha)
    )


# ---------------------------------------------------------------------------
# Optimal epsilon (closed-form for XOR/BF)
# ---------------------------------------------------------------------------

def optimal_epsilon_xor(alpha):
    """eps* = C*(1-alpha) / (delta*alpha*ln2)"""
    if alpha <= 0 or alpha >= 1:
        return None
    return XOR_C * (1 - alpha) / (DELTA * alpha * math.log(2))


def optimal_epsilon_binary_fuse(alpha, n_filter=None):
    """eps* = C*(1-alpha) / (delta*alpha*ln2)"""
    if alpha <= 0 or alpha >= 1:
        return None
    C = binary_fuse_C(n_filter)
    return C * (1 - alpha) / (DELTA * alpha * math.log(2))


# ---------------------------------------------------------------------------
# Best discrete params (grid search maximizing lower bound)
# ---------------------------------------------------------------------------

def best_discrete_xor(alpha, n_over_N, max_bits=8):
    best_lb, best_bits = float("-inf"), None
    for bits in range(1, max_bits + 1):
        b_e, eps = xor_params(bits)
        lb = lower_bound(alpha, eps, b_e, n_over_N)
        if lb > best_lb:
            best_lb, best_bits = lb, bits
    return best_bits, best_lb


def best_discrete_binary_fuse(alpha, n_over_N, n_filter=None, max_bits=8):
    best_lb, best_bits = float("-inf"), None
    for bits in range(1, max_bits + 1):
        b_e, eps = binary_fuse_params(bits, n_filter)
        lb = lower_bound(alpha, eps, b_e, n_over_N)
        if lb > best_lb:
            best_lb, best_bits = lb, bits
    return best_bits, best_lb


def best_discrete_bloom(alpha, n_over_N, k, max_bpe=8):
    best_lb, best_bpe = float("-inf"), None
    for bpe in range(1, max_bpe + 1):
        b_e, eps = bloom_params(bpe, k)
        lb = lower_bound(alpha, eps, b_e, n_over_N)
        if lb > best_lb:
            best_lb, best_bpe = lb, bpe
    return best_bpe, best_lb
