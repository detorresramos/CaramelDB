"""
Theoretical analysis of optimal Bloom filter parameters for CSF pre-filtering.

The key insight: we're minimizing TOTAL size, not just filter size.
The Bloom filter trades off:
- Filter size (grows with bits_per_element)
- False positives (shrink solution savings, decreases with bits_per_element)
"""

import numpy as np
import matplotlib.pyplot as plt


def bloom_fpr(bits_per_element: float, num_hashes: int = None) -> float:
    """
    Calculate Bloom filter false positive rate.

    If num_hashes not specified, uses optimal k = b * ln(2).

    FPR = (1 - e^(-k*n/m))^k
        ≈ (1 - e^(-k/b))^k

    With optimal k = b * ln(2):
        FPR ≈ 0.5^k = 0.5^(b * ln(2)) = 2^(-b * ln(2))
    """
    b = bits_per_element
    ln2 = np.log(2)

    if num_hashes is None:
        # Optimal k
        k = b * ln2
    else:
        k = num_hashes

    if b <= 0:
        return 1.0

    # Exact formula
    fpr = (1 - np.exp(-k / b)) ** k
    return fpr


def total_cost_per_key(alpha: float, bits_per_element: float,
                       solution_bits: float, num_hashes: int = None) -> float:
    """
    Calculate total bits per key for CSF with Bloom pre-filter.

    Components:
    - Filter: (1-α) * b bits per key (only minority keys in filter)
    - Solution: c * [(1-α) + α * FPR] bits per key
      - (1-α) minority keys must be in solution
      - α * FPR majority keys are false positives, also in solution

    Args:
        alpha: frequency of majority value
        bits_per_element: Bloom filter bits per element
        solution_bits: bits per key in the solution (from Huffman etc.)
        num_hashes: number of hash functions (None = optimal)

    Returns:
        Total bits per key
    """
    b = bits_per_element
    c = solution_bits
    fpr = bloom_fpr(b, num_hashes)

    filter_cost = (1 - alpha) * b
    solution_cost = c * ((1 - alpha) + alpha * fpr)

    return filter_cost + solution_cost


def optimal_bits_per_element(alpha: float, solution_bits: float) -> float:
    """
    Find optimal bits_per_element analytically.

    Minimizing: C(b) = (1-α)*b + c*[(1-α) + α * FPR(b)]

    With FPR(b) ≈ 2^(-b * ln(2)) for optimal k:

    dC/db = (1-α) - c * α * (ln2)² * 2^(-b*ln2) = 0

    Solving:
    2^(-b*ln2) = (1-α) / (c * α * (ln2)²)
    b = -log₂[(1-α) / (c * α * (ln2)²)] / ln(2)
    """
    ln2 = np.log(2)

    ratio = (1 - alpha) / (solution_bits * alpha * ln2**2)

    if ratio <= 0:
        return float('inf')  # No filter needed
    if ratio >= 1:
        return 0  # Filter not beneficial

    optimal_b = -np.log2(ratio) / ln2
    return max(0, optimal_b)


def optimal_num_hashes(bits_per_element: float) -> float:
    """Optimal number of hash functions for given bits_per_element."""
    return bits_per_element * np.log(2)


def plot_optimal_parameters(solution_bits: float = 8, output_path: str = None, show: bool = True):
    """
    Plot theoretical optimal Bloom parameters vs alpha.

    Args:
        solution_bits: bits per key in solution
        output_path: path to save plot
        show: whether to display
    """
    alphas = np.linspace(0.5, 0.99, 100)

    optimal_b = [optimal_bits_per_element(a, solution_bits) for a in alphas]
    optimal_k = [optimal_num_hashes(b) for b in optimal_b]

    # Also compute for discrete values (what we actually use)
    discrete_b = [max(1, min(8, round(b))) for b in optimal_b]
    discrete_k = [max(1, min(8, round(k))) for k in optimal_k]

    fig, axes = plt.subplots(2, 2, figsize=(14, 10))

    # Plot 1: Optimal bits_per_element (continuous)
    ax = axes[0, 0]
    ax.plot(alphas, optimal_b, 'b-', linewidth=2, label='Continuous optimal')
    ax.plot(alphas, discrete_b, 'r--', linewidth=2, label='Discrete (rounded)')
    ax.set_xlabel('α (frequency of majority value)')
    ax.set_ylabel('Optimal bits per element')
    ax.set_title(f'Optimal bits_per_element vs α (solution_bits={solution_bits})')
    ax.legend()
    ax.grid(True, alpha=0.3)
    ax.set_xlim(0.5, 1.0)

    # Plot 2: Optimal num_hashes (continuous)
    ax = axes[0, 1]
    ax.plot(alphas, optimal_k, 'g-', linewidth=2, label='Continuous optimal')
    ax.plot(alphas, discrete_k, 'r--', linewidth=2, label='Discrete (rounded)')
    ax.set_xlabel('α (frequency of majority value)')
    ax.set_ylabel('Optimal number of hashes')
    ax.set_title(f'Optimal num_hashes vs α (k = b × ln(2))')
    ax.legend()
    ax.grid(True, alpha=0.3)
    ax.set_xlim(0.5, 1.0)

    # Plot 3: Total cost breakdown for a specific alpha
    ax = axes[1, 0]
    alpha_example = 0.9
    b_range = np.linspace(0.1, 10, 100)

    filter_costs = [(1 - alpha_example) * b for b in b_range]
    fpr_values = [bloom_fpr(b) for b in b_range]
    solution_costs = [solution_bits * ((1 - alpha_example) + alpha_example * fpr) for fpr in fpr_values]
    total_costs = [f + s for f, s in zip(filter_costs, solution_costs)]

    ax.plot(b_range, filter_costs, 'b-', label='Filter cost', linewidth=2)
    ax.plot(b_range, solution_costs, 'g-', label='Solution cost', linewidth=2)
    ax.plot(b_range, total_costs, 'r-', label='Total cost', linewidth=2.5)

    # Mark optimal
    opt_b = optimal_bits_per_element(alpha_example, solution_bits)
    opt_total = total_cost_per_key(alpha_example, opt_b, solution_bits)
    ax.axvline(x=opt_b, color='k', linestyle='--', alpha=0.5)
    ax.scatter([opt_b], [opt_total], color='red', s=100, zorder=5, label=f'Optimal b={opt_b:.2f}')

    ax.set_xlabel('Bits per element (b)')
    ax.set_ylabel('Bits per key')
    ax.set_title(f'Cost breakdown at α={alpha_example}')
    ax.legend()
    ax.grid(True, alpha=0.3)

    # Plot 4: FPR vs bits_per_element
    ax = axes[1, 1]
    b_range = np.linspace(0.5, 10, 100)
    fpr_optimal_k = [bloom_fpr(b) for b in b_range]

    for k in [1, 2, 3, 4]:
        fpr_fixed_k = [bloom_fpr(b, k) for b in b_range]
        ax.semilogy(b_range, fpr_fixed_k, '--', alpha=0.7, label=f'k={k}')

    ax.semilogy(b_range, fpr_optimal_k, 'b-', linewidth=2.5, label='Optimal k = b×ln(2)')
    ax.set_xlabel('Bits per element (b)')
    ax.set_ylabel('False Positive Rate (log scale)')
    ax.set_title('Bloom FPR vs bits_per_element')
    ax.legend()
    ax.grid(True, alpha=0.3)

    plt.tight_layout()

    if output_path:
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        print(f'Saved: {output_path}')

    if show:
        plt.show()
    else:
        plt.close()


def print_theoretical_values(solution_bits: float = 8):
    """Print theoretical optimal values at key alpha points."""
    print(f"\nTheoretical optimal Bloom parameters (solution_bits={solution_bits})")
    print("=" * 60)
    print(f"{'Alpha':>8} | {'Opt b':>8} | {'Opt k':>8} | {'FPR':>12} | {'Round b':>8} | {'Round k':>8}")
    print("-" * 60)

    for alpha in [0.50, 0.60, 0.70, 0.80, 0.85, 0.90, 0.95, 0.98, 0.99]:
        opt_b = optimal_bits_per_element(alpha, solution_bits)
        opt_k = optimal_num_hashes(opt_b)
        fpr = bloom_fpr(opt_b)

        round_b = max(1, min(8, round(opt_b)))
        round_k = max(1, min(8, round(opt_k)))

        print(f"{alpha:>8.2f} | {opt_b:>8.2f} | {opt_k:>8.2f} | {fpr:>12.6f} | {round_b:>8d} | {round_k:>8d}")


def compare_with_empirical(empirical_alphas, empirical_opt_k, solution_bits=8):
    """
    Compare theoretical predictions with empirical results.

    Args:
        empirical_alphas: list of alpha values from experiments
        empirical_opt_k: list of optimal k values found empirically
        solution_bits: assumed bits per key in solution
    """
    theoretical_k = []
    for alpha in empirical_alphas:
        opt_b = optimal_bits_per_element(alpha, solution_bits)
        opt_k = optimal_num_hashes(opt_b)
        theoretical_k.append(opt_k)

    plt.figure(figsize=(10, 6))
    plt.plot(empirical_alphas, empirical_opt_k, 'bo-', label='Empirical', markersize=4)
    plt.plot(empirical_alphas, theoretical_k, 'r-', label='Theoretical', linewidth=2)
    plt.plot(empirical_alphas, [round(k) for k in theoretical_k], 'r--',
             label='Theoretical (rounded)', linewidth=1.5)

    plt.xlabel('α (frequency of majority value)')
    plt.ylabel('Optimal number of hashes (k)')
    plt.title('Empirical vs Theoretical Optimal num_hashes')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.show()


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description='Bloom filter theory analysis')
    parser.add_argument('--solution-bits', '-c', type=float, default=8,
                        help='Bits per key in solution')
    parser.add_argument('--output', '-o', type=str, default=None,
                        help='Output path for plot')
    parser.add_argument('--show', action='store_true', help='Show plot')

    args = parser.parse_args()

    print_theoretical_values(args.solution_bits)
    plot_optimal_parameters(args.solution_bits, args.output, args.show)
