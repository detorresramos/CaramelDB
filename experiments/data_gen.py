"""Data generation utilities for filter optimization experiments."""

import numpy as np

# Available minority distribution types
MINORITY_DISTRIBUTIONS = [
    "unique",
    "zipfian",
    "geometric",
    "uniform_10",
    "uniform_100",
    "two_value",
]


def gen_keys(n: int, seed: int = None) -> list[str]:
    """
    Generate n string keys.

    Args:
        n: Number of keys to generate
        seed: Random seed (unused for keys, included for API consistency)

    Returns:
        List of string keys like ["key0", "key1", ...]
    """
    return [f"key{i}" for i in range(n)]


def gen_alpha_values(
    n: int, alpha: float, seed: int = None, minority_dist: str = "unique"
) -> np.ndarray:
    """
    Generate values with specified alpha (frequency of most common element).

    The most common value is 0. Minority values depend on minority_dist:
    - "unique": Each minority key has a unique value (worst case)
    - "zipfian": Minority values follow Zipf distribution (s=1.5)
    - "uniform_10": Minority values uniformly from 10 distinct values
    - "uniform_100": Minority values uniformly from 100 distinct values
    - "two_value": Only one other value besides most common (best case)

    Args:
        n: Total number of values
        alpha: Frequency of most common element (0.0 to 1.0)
        seed: Random seed for shuffling
        minority_dist: Distribution type for minority values

    Returns:
        np.ndarray of uint32 values
    """
    if seed is not None:
        np.random.seed(seed)

    num_most_common = int(n * alpha)
    num_minority = n - num_most_common

    most_common_value = 0

    values = np.zeros(n, dtype=np.uint32)
    values[:num_most_common] = most_common_value

    if num_minority > 0:
        minority_values = _generate_minority_values(num_minority, minority_dist)
        values[num_most_common:] = minority_values

    np.random.shuffle(values)

    return values


def _generate_minority_values(num_minority: int, minority_dist: str) -> np.ndarray:
    """
    Generate minority values according to the specified distribution.

    All values are offset by 1 to avoid collision with most_common_value=0.
    """
    if minority_dist == "unique":
        # Each minority key has a unique value (worst case for codebook)
        return np.arange(1, num_minority + 1, dtype=np.uint32)

    elif minority_dist == "zipfian":
        # Zipf distribution - some values much more common than others
        # Using s=1.5 for moderate skew
        raw = np.random.zipf(1.5, size=num_minority)
        return raw.astype(np.uint32)

    elif minority_dist == "geometric":
        # Geometric distribution - exponentially decaying probabilities
        # p=0.3 gives moderate spread
        raw = np.random.geometric(0.3, size=num_minority)
        return raw.astype(np.uint32)

    elif minority_dist == "uniform_10":
        # Uniform from 10 distinct values
        return np.random.randint(1, 11, size=num_minority, dtype=np.uint32)

    elif minority_dist == "uniform_100":
        # Uniform from 100 distinct values
        return np.random.randint(1, 101, size=num_minority, dtype=np.uint32)

    elif minority_dist == "two_value":
        # Only one other value - best case for codebook
        return np.ones(num_minority, dtype=np.uint32)

    else:
        raise ValueError(f"Unknown minority distribution: {minority_dist}")


def compute_actual_alpha(values: np.ndarray) -> float:
    """
    Compute the actual alpha (frequency of most common element).

    Args:
        values: Array of values

    Returns:
        Actual frequency of the most common element
    """
    unique, counts = np.unique(values, return_counts=True)
    return float(counts.max()) / len(values)


def get_most_common_value(values: np.ndarray) -> int:
    """
    Get the most common value in the array.

    Args:
        values: Array of values

    Returns:
        The most common value
    """
    unique, counts = np.unique(values, return_counts=True)
    return int(unique[counts.argmax()])


def count_minority_keys(values: np.ndarray) -> int:
    """
    Count the number of keys that don't have the most common value.
    This is the number of elements that will be stored in the filter.

    Args:
        values: Array of values

    Returns:
        Number of minority keys
    """
    most_common = get_most_common_value(values)
    return int(np.sum(values != most_common))
