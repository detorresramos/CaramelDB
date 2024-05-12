import glob
import os
import warnings
from pathlib import Path

import numpy as np

from ._caramel import (
    CSFChar10,
    CSFChar12,
    CsfDeserializationException,
    CSFString,
    CSFUint32,
    CSFUint64,
    MultisetCSFChar10,
    MultisetCSFChar12,
    MultisetCSFString,
    MultisetCSFUint32,
    MultisetCSFUint64,
)

CLASS_LIST = [
    CSFChar10,
    CSFChar12,
    CSFString,
    CSFUint32,
    CSFUint64,
    MultisetCSFChar10,
    MultisetCSFChar12,
    MultisetCSFString,
    MultisetCSFUint32,
    MultisetCSFUint64,
]


def Caramel(
    keys,
    values,
    use_bloom_filter=True,
    permute=False,
    max_to_infer=None,
    verbose=True,
):
    """
    Constructs a Caramel object, automatically inferring the correct CSF backend.

    Arguments:
        keys: List of hashable keys.
        values: List of values to use in the CSF.
        max_to_infer: If provided, only the first "max_to_infer" values
            will be examinied when inferring the correct CSF backend.

    Returns:
        A CSF containing the desired key-value mapping.

    Raises:
        ValueError if the keys and values cannot be used to construct a CSF.
    """
    if not len(keys):
        raise ValueError("Keys must be non-empty but found length 0.")
    if not len(values):
        raise ValueError("Values must be non-empty but found length 0.")
    if len(keys) != len(values):
        raise ValueError("Keys and values must have the same length.")
    if not isinstance(keys[0], (str, bytes)):
        raise ValueError(f"Keys must be str or bytes, found {type(keys[0])}")

    try:
        warnings.filterwarnings("error", category=np.VisibleDeprecationWarning)
        values = np.array(values)
    except Exception:
        raise ValueError(
            "Error transforming values to numpy array. Make sure all rows are the same length."
        )

    CSFClass = _infer_backend(values, max_to_infer=max_to_infer)
    if CSFClass.is_multiset():
        values = values.T
        csf = CSFClass(
            keys,
            values,
            permute=permute,
            use_bloom_filter=use_bloom_filter,
            verbose=verbose,
        )
    else:
        csf = CSFClass(keys, values, use_bloom_filter=use_bloom_filter, verbose=verbose)
    csf = _wrap_backend(csf)
    return csf


def load(filename):
    """
    Loads a CSF from file.

    Arguments:
        filename: File containing a serialized CSF.

    Returns:
        A CSF of the correct sub-type.

    Raises:
        ValueError if the filename does not contain a valid CSF.
    """
    for csf_class in CLASS_LIST:
        try:
            csf = csf_class.load(filename)
            return _wrap_backend(csf)
        except CsfDeserializationException as e:
            continue
    raise ValueError(f"File {filename} does not contain a deserializable CSF.")


class CSFQueryWrapper(object):
    """Wraps a CSF, applying a postprocessing function to the query results."""

    def __init__(self, csf, postprocess_fn):
        self._csf = csf
        self._postprocess_fn = postprocess_fn

    def query(self, q):
        return self._postprocess_fn(self._csf.query(q))

    def __getattr__(self, name):
        return getattr(self._csf, name)


def _infer_backend(values, max_to_infer=None):
    """Returns a CSF class, selected based on the key / value types."""

    if values.ndim == 1:
        value_to_test = values[0]
        multiset = False
    elif values.ndim == 2:
        if len(values[0]) == 0:
            raise ValueError("Subarray must not be empty.")
        value_to_test = values[0][0]
        multiset = True
    else:
        raise ValueError("Caramel only supports 1D and 2D arrays as values.")

    if np.issubdtype(type(value_to_test), np.integer):
        if np.issubdtype(type(value_to_test), np.uint64):
            return MultisetCSFUint64 if multiset else CSFUint64
        return MultisetCSFUint32 if multiset else CSFUint32

    if isinstance(value_to_test, (str, bytes)):
        value_length = _infer_length(values, max_to_infer)
        if value_length == 10:
            return MultisetCSFChar10 if multiset else CSFChar10
        elif value_length == 12:
            return MultisetCSFChar12 if multiset else CSFChar12
        return MultisetCSFString if multiset else CSFString

    raise ValueError(f"Unsupported value type: {type(value_to_test).__name__}")


def _infer_length(values_to_infer, max_to_infer):
    if values_to_infer.ndim == 1:
        subset = values_to_infer[:max_to_infer]
    elif values_to_infer.ndim == 2:
        subset = values_to_infer[:max_to_infer, 0]
    else:
        raise ValueError("Input array must be either 1D or 2D.")

    lengths = np.vectorize(len)(subset)

    if np.all(lengths == lengths[0]):
        return lengths[0]
    else:
        return None


def _wrap_backend(csf):
    """Wraps the backend CSF (e.g., to apply post-query processing)."""
    list_to_str_classes = (CSFChar10, CSFChar12)

    if isinstance(csf, list_to_str_classes):
        csf = CSFQueryWrapper(csf, lambda x: "".join(x))

    return csf


class MultisetCSF:
    def __init__(
        self,
        keys,
        values,
        max_to_infer=None,
        multiset_permute_optimization=False,
        use_bloom_filter=True,
        verbose=True,
    ):
        try:
            warnings.filterwarnings("error", category=np.VisibleDeprecationWarning)
            values = np.array(values)
        except Exception:
            raise ValueError(
                "Error transforming values to numpy array. Make sure all rows are the same length."
            )

        if multiset_permute_optimization:
            values = permute_values(values)

        values = values.T

        self._csfs = []
        for i in range(len(values)):
            self._csfs.append(
                Caramel(
                    keys,
                    values[i],
                    max_to_infer,
                    use_bloom_filter=use_bloom_filter,
                    verbose=verbose,
                )
            )

    def query(self, key):
        return [csf.query(key) for csf in self._csfs]

    def save(self, filename):
        directory = Path(filename)
        os.mkdir(directory)

        for i, csf in enumerate(self._csfs):
            csf.save(str(directory / f"column_{i}.csf"))

    @classmethod
    def load(cls, filename):
        instance = cls.__new__(cls)
        directory = Path(filename)

        csf_files = sorted(
            glob.glob(str(directory / "*.csf")),
            key=lambda x: int(x.split("/")[-1].split("_")[-1].split(".")[0]),
        )

        instance._csfs = []
        for i, column_csf_file in enumerate(csf_files):
            assert column_csf_file.split("/")[-1] == f"column_{i}.csf"
            instance._csfs.append(load(column_csf_file))

        return instance
