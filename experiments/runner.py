#!/usr/bin/env python3
"""
Experiment runner for filter optimization.

Usage:
    python runner.py grid --output results/grid_search.json
    python runner.py sweep --n 100000 --filter binary_fuse --fingerprint-bits 8
    python runner.py crossover --n 100000 --filter binary_fuse --fingerprint-bits 8
    python runner.py grid --n 1000,10000 --alpha 0.7,0.8,0.9 --fingerprint-bits 8,12
"""

import argparse
import json
import os
import subprocess
from dataclasses import asdict, dataclass, field
from datetime import datetime
from typing import Optional

from data_gen import MINORITY_DISTRIBUTIONS, gen_alpha_values, gen_keys
from measure import ExperimentResult, measure_csf


@dataclass
class ExperimentConfig:
    """Configurable hyperparameters for experiments."""

    n_values: list[int] = field(default_factory=lambda: [1_000, 10_000, 100_000])
    alpha_values: list[float] = field(
        default_factory=lambda: [
            0.50,
            0.55,
            0.60,
            0.65,
            0.70,
            0.75,
            0.80,
            0.85,
            0.90,
            0.95,
            0.99,
        ]
    )

    # XOR/BinaryFuse
    fingerprint_bits_values: list[int] = field(
        default_factory=lambda: [4, 6, 8, 10, 12, 16]
    )

    # Bloom
    bloom_bits_per_element: list[int] = field(default_factory=lambda: [8, 10, 12, 16])
    bloom_num_hashes: list[int] = field(default_factory=lambda: [3, 5, 7])

    # Minority distributions
    minority_dists: list[str] = field(default_factory=lambda: ["unique"])

    seed: int = 42
    output_dir: str = "experiments/results"

    def to_dict(self) -> dict:
        return asdict(self)


def get_git_commit() -> Optional[str]:
    """Get current git commit hash."""
    try:
        result = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            capture_output=True,
            text=True,
            check=True,
        )
        return result.stdout.strip()[:12]
    except Exception:
        return None


def enumerate_filter_configs(config: ExperimentConfig) -> list[dict]:
    """
    Enumerate all filter configurations to test.

    Returns:
        List of dicts with filter_type and parameters
    """
    configs = []

    # No filter baseline
    configs.append({"filter_type": "none"})

    # XOR filters
    for bits in config.fingerprint_bits_values:
        configs.append({"filter_type": "xor", "fingerprint_bits": bits})

    # BinaryFuse filters
    for bits in config.fingerprint_bits_values:
        configs.append({"filter_type": "binary_fuse", "fingerprint_bits": bits})

    # Bloom filters
    for bits_per_elem in config.bloom_bits_per_element:
        for num_hashes in config.bloom_num_hashes:
            configs.append(
                {
                    "filter_type": "bloom",
                    "bloom_bits_per_element": bits_per_elem,
                    "bloom_num_hashes": num_hashes,
                }
            )

    return configs


def run_grid_search(config: ExperimentConfig) -> list[ExperimentResult]:
    """
    Run full grid search over all configurations.

    Args:
        config: ExperimentConfig with hyperparameters

    Returns:
        List of ExperimentResult for all configurations
    """
    results = []
    filter_configs = enumerate_filter_configs(config)
    total = (
        len(config.n_values)
        * len(config.alpha_values)
        * len(filter_configs)
        * len(config.minority_dists)
    )

    print(f"Running grid search: {total} configurations")
    print(f"  N values: {config.n_values}")
    print(f"  Alpha values: {config.alpha_values}")
    print(f"  Filter configs: {len(filter_configs)}")
    print(f"  Minority distributions: {config.minority_dists}")
    print()

    count = 0
    for minority_dist in config.minority_dists:
        for n in config.n_values:
            keys = gen_keys(n)

            for alpha in config.alpha_values:
                values = gen_alpha_values(
                    n, alpha, seed=config.seed, minority_dist=minority_dist
                )

                for fc in filter_configs:
                    count += 1
                    filter_type = fc["filter_type"]
                    fingerprint_bits = fc.get("fingerprint_bits")
                    bloom_bits_per_element = fc.get("bloom_bits_per_element")
                    bloom_num_hashes = fc.get("bloom_num_hashes")

                    # Progress indicator
                    config_str = filter_type
                    if fingerprint_bits:
                        config_str += f"_{fingerprint_bits}"
                    if bloom_bits_per_element:
                        config_str += f"_{bloom_bits_per_element}_{bloom_num_hashes}"

                    dist_str = (
                        f"[{minority_dist}]" if len(config.minority_dists) > 1 else ""
                    )
                    print(
                        f"[{count}/{total}] {dist_str}n={n:,}, alpha={alpha:.2f}, {config_str}",
                        end="",
                        flush=True,
                    )

                    try:
                        result = measure_csf(
                            keys=keys,
                            values=values,
                            filter_type=filter_type,
                            n=n,
                            alpha=alpha,
                            fingerprint_bits=fingerprint_bits,
                            bloom_bits_per_element=bloom_bits_per_element,
                            bloom_num_hashes=bloom_num_hashes,
                            minority_dist=minority_dist,
                        )
                        results.append(result)
                        print(
                            f" -> {result.total_bytes:,} bytes ({result.bits_per_key:.2f} bpk)"
                        )
                    except Exception as e:
                        print(f" -> ERROR: {e}")

    return results


def run_alpha_sweep(
    n: int,
    filter_type: str,
    alpha_values: list[float],
    seed: int = 42,
    fingerprint_bits: Optional[int] = None,
    bloom_bits_per_element: Optional[int] = None,
    bloom_num_hashes: Optional[int] = None,
) -> list[ExperimentResult]:
    """
    Run sweep over alpha for a single filter configuration.

    Args:
        n: Number of keys
        filter_type: 'none', 'xor', 'binary_fuse', or 'bloom'
        alpha_values: List of alpha values to test
        seed: Random seed
        fingerprint_bits: For XOR/BinaryFuse
        bloom_bits_per_element: For Bloom
        bloom_num_hashes: For Bloom

    Returns:
        List of ExperimentResult
    """
    results = []
    keys = gen_keys(n)

    for alpha in alpha_values:
        values = gen_alpha_values(n, alpha, seed=seed)

        result = measure_csf(
            keys=keys,
            values=values,
            filter_type=filter_type,
            n=n,
            alpha=alpha,
            fingerprint_bits=fingerprint_bits,
            bloom_bits_per_element=bloom_bits_per_element,
            bloom_num_hashes=bloom_num_hashes,
        )
        results.append(result)

    return results


def find_crossover_alpha(
    n: int,
    filter_type: str,
    seed: int = 42,
    precision: float = 0.01,
    fingerprint_bits: Optional[int] = None,
    bloom_bits_per_element: Optional[int] = None,
    bloom_num_hashes: Optional[int] = None,
) -> Optional[float]:
    """
    Binary search to find alpha where filter becomes beneficial.

    Args:
        n: Number of keys
        filter_type: 'xor', 'binary_fuse', or 'bloom'
        seed: Random seed
        precision: Search precision
        fingerprint_bits: For XOR/BinaryFuse
        bloom_bits_per_element: For Bloom
        bloom_num_hashes: For Bloom

    Returns:
        Alpha threshold or None if filter never beneficial
    """
    keys = gen_keys(n)

    def is_filter_beneficial(alpha: float) -> bool:
        values = gen_alpha_values(n, alpha, seed=seed)

        try:
            # With filter
            result_with = measure_csf(
                keys=keys,
                values=values,
                filter_type=filter_type,
                n=n,
                alpha=alpha,
                fingerprint_bits=fingerprint_bits,
                bloom_bits_per_element=bloom_bits_per_element,
                bloom_num_hashes=bloom_num_hashes,
            )
        except ValueError:
            # Filter construction failed (e.g., BinaryFuse needs >10 elements)
            return False

        # Without filter
        result_without = measure_csf(
            keys=keys,
            values=values,
            filter_type="none",
            n=n,
            alpha=alpha,
        )

        return result_with.total_bytes < result_without.total_bytes

    # Quick check: is filter beneficial at high alpha (0.95 to allow for filter min elements)?
    if not is_filter_beneficial(0.95):
        return None

    # Binary search
    left, right = 0.5, 0.95
    while (right - left) > precision:
        mid = (left + right) / 2
        if is_filter_beneficial(mid):
            right = mid
        else:
            left = mid

    return (left + right) / 2


def save_results(
    results: list[ExperimentResult],
    filepath: str,
    config: Optional[ExperimentConfig] = None,
):
    """
    Save results to JSON with metadata.

    Args:
        results: List of ExperimentResult
        filepath: Output file path
        config: Optional ExperimentConfig for metadata
    """
    os.makedirs(os.path.dirname(filepath) or ".", exist_ok=True)

    data = {
        "metadata": {
            "timestamp": datetime.now().isoformat(),
            "git_commit": get_git_commit(),
            "num_results": len(results),
            "config": config.to_dict() if config else None,
        },
        "results": [r.to_dict() for r in results],
    }

    with open(filepath, "w") as f:
        json.dump(data, f, indent=2)

    print(f"\nSaved {len(results)} results to {filepath}")


def load_results(filepath: str) -> list[ExperimentResult]:
    """
    Load results from JSON.

    Args:
        filepath: Input file path

    Returns:
        List of ExperimentResult
    """
    with open(filepath, "r") as f:
        data = json.load(f)

    return [ExperimentResult.from_dict(r) for r in data["results"]]


def parse_list_arg(value: str, type_fn=str) -> list:
    """Parse comma-separated argument into list."""
    return [type_fn(x.strip()) for x in value.split(",")]


def main():
    parser = argparse.ArgumentParser(
        description="Filter optimization experiments",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    # Grid search command
    p_grid = subparsers.add_parser("grid", help="Run full grid search")
    p_grid.add_argument(
        "-o", "--output", default="experiments/results/grid_search.json"
    )
    p_grid.add_argument("--n", type=str, help="Comma-separated N values")
    p_grid.add_argument("--alpha", type=str, help="Comma-separated alpha values")
    p_grid.add_argument(
        "--fingerprint-bits", type=str, help="Comma-separated fingerprint bits"
    )
    p_grid.add_argument(
        "--bloom-bits-per-element",
        type=str,
        help="Comma-separated bloom bits per element",
    )
    p_grid.add_argument(
        "--bloom-num-hashes", type=str, help="Comma-separated bloom num hashes"
    )
    p_grid.add_argument(
        "--minority-dist",
        type=str,
        help=f"Comma-separated minority distributions: {','.join(MINORITY_DISTRIBUTIONS)}",
    )
    p_grid.add_argument("--seed", type=int, default=42)

    # Alpha sweep command
    p_sweep = subparsers.add_parser("sweep", help="Run alpha sweep for single config")
    p_sweep.add_argument("--n", type=int, required=True)
    p_sweep.add_argument(
        "--filter",
        type=str,
        required=True,
        choices=["none", "xor", "binary_fuse", "bloom"],
    )
    p_sweep.add_argument("--fingerprint-bits", type=int)
    p_sweep.add_argument("--bloom-bits-per-element", type=int)
    p_sweep.add_argument("--bloom-num-hashes", type=int)
    p_sweep.add_argument("--alpha", type=str, help="Comma-separated alpha values")
    p_sweep.add_argument("--seed", type=int, default=42)
    p_sweep.add_argument("-o", "--output", help="Output file path")

    # Crossover command
    p_cross = subparsers.add_parser("crossover", help="Find crossover alpha")
    p_cross.add_argument("--n", type=int, required=True)
    p_cross.add_argument(
        "--filter", type=str, required=True, choices=["xor", "binary_fuse", "bloom"]
    )
    p_cross.add_argument("--fingerprint-bits", type=int)
    p_cross.add_argument("--bloom-bits-per-element", type=int)
    p_cross.add_argument("--bloom-num-hashes", type=int)
    p_cross.add_argument("--precision", type=float, default=0.01)
    p_cross.add_argument("--seed", type=int, default=42)

    args = parser.parse_args()

    if args.command == "grid":
        config = ExperimentConfig(seed=args.seed)

        if args.n:
            config.n_values = parse_list_arg(args.n, int)
        if args.alpha:
            config.alpha_values = parse_list_arg(args.alpha, float)
        if args.fingerprint_bits:
            config.fingerprint_bits_values = parse_list_arg(args.fingerprint_bits, int)
        if args.bloom_bits_per_element:
            config.bloom_bits_per_element = parse_list_arg(
                args.bloom_bits_per_element, int
            )
        if args.bloom_num_hashes:
            config.bloom_num_hashes = parse_list_arg(args.bloom_num_hashes, int)
        if args.minority_dist:
            config.minority_dists = parse_list_arg(args.minority_dist, str)

        results = run_grid_search(config)
        save_results(results, args.output, config)

    elif args.command == "sweep":
        default_alphas = [
            0.50,
            0.55,
            0.60,
            0.65,
            0.70,
            0.75,
            0.80,
            0.85,
            0.90,
            0.95,
            0.99,
        ]
        alpha_values = (
            parse_list_arg(args.alpha, float) if args.alpha else default_alphas
        )

        print(f"Running alpha sweep: n={args.n:,}, filter={args.filter}")
        results = run_alpha_sweep(
            n=args.n,
            filter_type=args.filter,
            alpha_values=alpha_values,
            seed=args.seed,
            fingerprint_bits=args.fingerprint_bits,
            bloom_bits_per_element=args.bloom_bits_per_element,
            bloom_num_hashes=args.bloom_num_hashes,
        )

        print("\nResults:")
        print(f"{'Alpha':>8} {'Total Bytes':>14} {'Bits/Key':>10}")
        print("-" * 36)
        for r in results:
            print(f"{r.alpha:>8.2f} {r.total_bytes:>14,} {r.bits_per_key:>10.2f}")

        if args.output:
            save_results(results, args.output)

    elif args.command == "crossover":
        print(f"Finding crossover alpha: n={args.n:,}, filter={args.filter}")

        threshold = find_crossover_alpha(
            n=args.n,
            filter_type=args.filter,
            seed=args.seed,
            precision=args.precision,
            fingerprint_bits=args.fingerprint_bits,
            bloom_bits_per_element=args.bloom_bits_per_element,
            bloom_num_hashes=args.bloom_num_hashes,
        )

        if threshold is None:
            print(f"Filter is never beneficial for n={args.n:,}")
        else:
            print(f"Crossover alpha: {threshold:.3f}")
            print(f"Filter is beneficial when alpha >= {threshold:.3f}")


if __name__ == "__main__":
    main()
