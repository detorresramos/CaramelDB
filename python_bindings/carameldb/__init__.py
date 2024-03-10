import glob
import os
import re
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

multiset_csf_classes = (
    MultisetCSFChar10,
    MultisetCSFChar12,
    MultisetCSFString,
    MultisetCSFUint32,
    MultisetCSFUint64,
)

csf_classes = (
    CSFUint32,
    CSFUint64,
    CSFChar10,
    CSFChar12,
    CSFString,
) + multiset_csf_classes


def Caramel(
    keys,
    values,
    max_to_infer=None,
    permute=False,
    use_bloom_filter=True,
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
        print("LMFAO", len(keys), len(values))
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

    CSFClass = _infer_backend(keys, values, max_to_infer=max_to_infer)
    if CSFClass in multiset_csf_classes:
        values = values.T
        print(values)
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
    for csf_class in csf_classes:
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


def _infer_backend(keys, values, max_to_infer=None):
    """Returns a CSF class, selected based on the key / value types."""

    if isinstance(values[0], (list, np.ndarray)):
        return _infer_multiset(values, max_to_infer)

    if np.issubdtype(type(values[0]), np.integer):
        if np.issubdtype(type(values[0]), np.uint64):
            return CSFUint64
        return CSFUint32

    if isinstance(values[0], (str, bytes)):
        # call out to one of the dedicated-length strings
        validate_values = values[:max_to_infer] if max_to_infer else values
        value_length = _infer_length(validate_values)
        if value_length == 10:
            return CSFChar10
        elif value_length == 12:
            return CSFChar12
        else:
            return CSFString

    raise ValueError(f"Unsupported value type: {type(values[0])}")


def _infer_multiset(values, max_to_infer):
    if len(values[0]) == 0:
        raise ValueError("Cannot have empty list as value.")

    if np.issubdtype(type(values[0][0]), np.integer):
        if np.issubdtype(type(values[0][0]), np.uint64):
            return MultisetCSFUint64
        return MultisetCSFUint32

    print("GOT PAST MULTISET INT CHECK")

    if isinstance(values[0][0], (str, bytes)):
        # call out to one of the dedicated-length strings
        validate_rows = values[:max_to_infer] if max_to_infer else values
        validate_values = []
        for row in validate_rows:
            if len(row) == 0:
                raise ValueError("Cannot have empty list as value.")
            validate_values.append(row[0])
        value_length = _infer_length(validate_values)
        if value_length == 10:
            return MultisetCSFChar10
        elif value_length == 12:
            return MultisetCSFChar12
        else:
            return MultisetCSFString

    raise ValueError(f"Unsupported value type: {type(values[0])}")


def _infer_length(values):
    """Returns the length of each value, if all values have the same length."""
    target_length = len(values[0])
    for v in values:
        if len(v) != target_length:
            return None
    return target_length


def _wrap_backend(csf):
    """Wraps the backend CSF (e.g., to apply post-query processing)."""
    list_to_str_classes = (CSFChar10, CSFChar12)

    if isinstance(csf, list_to_str_classes):
        csf = CSFQueryWrapper(csf, lambda x: "".join(x))

    return csf
