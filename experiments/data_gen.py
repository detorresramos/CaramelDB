"""Data generation utilities for filter optimization experiments."""

import numpy as np

MINORITY_DISTRIBUTIONS = [
    "unique",
    "zipfian",
    "geometric",
    "uniform_10",
    "uniform_100",
    "two_value",
]


def gen_keys(n: int, seed: int = None) -> list[str]:
    return [f"key{i}" for i in range(n)]


def gen_alpha_values(
    n: int, alpha: float, seed: int = None, minority_dist: str = "unique"
) -> np.ndarray:
    """Generate values where the most common value (0) has frequency alpha.

    minority_dist controls the non-majority values:
      "unique"      - each minority key has a unique value (worst case)
      "zipfian"     - Zipf distribution (s=1.5)
      "uniform_10"  - uniform from 10 distinct values
      "uniform_100" - uniform from 100 distinct values
      "two_value"   - single other value (best case)
    """
    if seed is not None:
        np.random.seed(seed)

    num_most_common = int(n * alpha)
    num_minority = n - num_most_common

    values = np.zeros(n, dtype=np.uint32)

    if num_minority > 0:
        minority_values = _generate_minority_values(num_minority, minority_dist)
        values[num_most_common:] = minority_values

    np.random.shuffle(values)

    return values


def _generate_minority_values(num_minority: int, minority_dist: str) -> np.ndarray:
    # Values start at 1 to avoid collision with most_common_value=0
    if minority_dist == "unique":
        return np.arange(1, num_minority + 1, dtype=np.uint32)
    elif minority_dist == "zipfian":
        return np.random.zipf(1.5, size=num_minority).astype(np.uint32)
    elif minority_dist == "geometric":
        return np.random.geometric(0.3, size=num_minority).astype(np.uint32)
    elif minority_dist == "uniform_10":
        return np.random.randint(1, 11, size=num_minority, dtype=np.uint32)
    elif minority_dist == "uniform_100":
        return np.random.randint(1, 101, size=num_minority, dtype=np.uint32)
    elif minority_dist == "two_value":
        return np.ones(num_minority, dtype=np.uint32)
    else:
        raise ValueError(f"Unknown minority distribution: {minority_dist}")


def compute_actual_alpha(values: np.ndarray) -> float:
    _, counts = np.unique(values, return_counts=True)
    return float(counts.max()) / len(values)


def get_most_common_value(values: np.ndarray) -> int:
    unique, counts = np.unique(values, return_counts=True)
    return int(unique[counts.argmax()])


def count_minority_keys(values: np.ndarray) -> int:
    most_common = get_most_common_value(values)
    return int(np.sum(values != most_common))
