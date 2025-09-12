import numpy as np

from ._caramel import (
    BloomFilterConfig,
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
    XORFilterConfig,
    permute_char10,
    permute_char12,
    permute_uint32,
    permute_uint64,
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
    prefilter=None,
    permute=False,
    max_to_infer=None,
    verbose=True,
):
    """
    Constructs a Caramel object, automatically inferring the correct CSF backend.

    Arguments:
        keys: List of hashable keys.
        values: List of values to use in the CSF.
        prefilter: The type of prefilter to use.
        permute: If true, permutes rows of matrix inputs to minimize entropy.
        max_to_infer: If provided, only the first "max_to_infer" values
            will be examinied when inferring the correct CSF backend.
        verbose: Enable verbose logging

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
        values = np.array(values)
    except Exception:
        raise ValueError(
            "Error transforming values to numpy array. Make sure all rows are the same length."
        )

    CSFClass = _infer_backend(values, max_to_infer=max_to_infer)
    if CSFClass.is_multiset():
        if permute:
            values = permute_values(values, csf_class_type=CSFClass)

        try:
            values = values.T
        except Exception:
            raise ValueError(
                "Error transforming values to column-wise. Make sure all values are the same length."
            )

        csf = CSFClass(
            keys,
            values,
            prefilter=prefilter,
            verbose=verbose,
        )
    else:
        csf = CSFClass(keys, values, prefilter=prefilter, verbose=verbose)
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

    def __dir__(self):
        """Return the list of attributes of the wrapped object and this wrapper to aid with introspection."""
        return sorted(set(dir(self._csf) + super().__dir__()))

    def __repr__(self):
        """Return a string representation of the wrapper that identifies the wrapped object."""
        return repr(self._csf)


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


def permute_values(values, csf_class_type):
    if csf_class_type == MultisetCSFChar10:
        values = values.astype("|S10")
        permute_char10(values)
        return values
    elif csf_class_type == MultisetCSFChar12:
        values = values.astype("|S12")
        permute_char12(values)
        return values
    elif csf_class_type == MultisetCSFUint32:
        values = values.astype(np.uint32)
        permute_uint32(values)
        return values
    elif csf_class_type == MultisetCSFUint64:
        values = values.astype(np.uint64)
        permute_uint64(values)
        return values
    else:
        raise ValueError("'permute' flag not supported for this multiset class type.")
