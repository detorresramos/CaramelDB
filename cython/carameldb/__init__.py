import numpy as np

from ._caramel import (
    AutoFilterConfig,
    EntropyPermutationConfig,
    GlobalSortPermutationConfig,
    BinaryFuseFilterConfig,
    BinaryFusePreFilterUint32,
    BinaryFusePreFilterUint64,
    BloomFilterConfig,
    BloomPreFilterChar10,
    BloomPreFilterChar12,
    BloomPreFilterString,
    BloomPreFilterUint32,
    BloomPreFilterUint64,
    BucketStats,
    CSFChar10,
    CSFChar12,
    CsfDeserializationException,
    CsfStats,
    CSFString,
    CSFUint32,
    CSFUint64,
    FilterStats,
    HuffmanStats,
    MultisetCSFChar10,
    MultisetCSFChar12,
    MultisetCSFString,
    MultisetCSFUint32,
    MultisetCSFUint64,
    PackedCSFUint32,
    RaggedMultisetCSFUint32,
    PreFilterChar10,
    PreFilterChar12,
    PreFilterString,
    PreFilterUint32,
    PreFilterUint64,
    UnorderedMapBaseline,
    XORFilterConfig,
    XORPreFilterUint32,
    XORPreFilterUint64,
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
    prefilter=AutoFilterConfig(),
    permutation=None,
    shared_codebook=False,
    shared_filter=False,
    max_to_infer=None,
    verbose=True,
):
    """
    Constructs a Caramel object, automatically inferring the correct CSF backend.

    Arguments:
        keys: List of hashable keys.
        values: List of values to use in the CSF.
        prefilter: The type of prefilter to use.
        permutation: A PermutationConfig specifying the permutation algorithm.
        shared_codebook: If true, uses a single shared Huffman codebook across
            all columns. Only valid for multiset (2D) values.
        shared_filter: If true, columns that share the same most common value
            (MCV) use a single shared prefilter. Only valid for multiset (2D)
            values. Requires prefilter.
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
        csf = CSFClass(
            keys, values, prefilter=prefilter, permutation=permutation,
            shared_codebook=shared_codebook, shared_filter=shared_filter,
            verbose=verbose,
        )
    else:
        if permutation is not None:
            raise ValueError("'permutation' is only supported for multiset (2D) values.")
        if shared_codebook:
            raise ValueError("'shared_codebook' is only supported for multiset (2D) values.")
        if shared_filter:
            raise ValueError("'shared_filter' is only supported for multiset (2D) values.")
        csf = CSFClass(keys, values, prefilter=prefilter, verbose=verbose)
    return csf


def CaramelRagged(
    keys,
    values,
    prefilter=AutoFilterConfig(),
    permutation=None,
    shared_codebook=False,
    verbose=True,
):
    """
    Constructs a RaggedMultisetCSF: a length CSF + per-column CSFs where
    each column only includes keys whose array reaches that column.

    Arguments:
        keys: List of hashable keys.
        values: List of lists of uint32 values (must be ragged or fixed-length).
        prefilter: The type of prefilter to use.
        permutation: A PermutationConfig for the min_length prefix columns.
        shared_codebook: If true, uses a single shared Huffman codebook.
        verbose: Enable verbose logging.

    Returns:
        A RaggedMultisetCSF containing the desired key-value mapping.
    """
    if not len(keys):
        raise ValueError("Keys must be non-empty.")
    if not len(values):
        raise ValueError("Values must be non-empty.")
    if len(keys) != len(values):
        raise ValueError("Keys and values must have the same length.")
    if not isinstance(keys[0], (str, bytes)):
        raise ValueError(f"Keys must be str or bytes, found {type(keys[0])}")

    # Convert numpy 2D array to list-of-lists
    if isinstance(values, np.ndarray):
        values = [row.tolist() for row in values]

    return RaggedMultisetCSFUint32(
        keys, values, prefilter=prefilter, permutation=permutation,
        shared_codebook=shared_codebook, verbose=verbose,
    )


def CaramelPacked(keys, values, verbose=True):
    """
    Constructs a PackedCSF: a single CSF storing concatenated Huffman codes
    with a stop symbol. Works for both fixed-length and ragged (variable-length)
    value arrays.

    Arguments:
        keys: List of hashable keys.
        values: List of lists of uint32 values (can be ragged).
        verbose: Enable verbose logging.

    Returns:
        A PackedCSF containing the desired key-value mapping.
    """
    if not len(keys):
        raise ValueError("Keys must be non-empty.")
    if not len(values):
        raise ValueError("Values must be non-empty.")
    if len(keys) != len(values):
        raise ValueError("Keys and values must have the same length.")
    if not isinstance(keys[0], (str, bytes)):
        raise ValueError(f"Keys must be str or bytes, found {type(keys[0])}")

    # Convert to list-of-lists if numpy 2D array
    if isinstance(values, np.ndarray):
        values = [row.tolist() for row in values]

    return PackedCSFUint32(keys, values, verbose=verbose)


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
            return csf_class.load(filename)
        except CsfDeserializationException as e:
            continue
    raise ValueError(f"File {filename} does not contain a deserializable CSF.")


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
        if np.issubdtype(type(value_to_test), np.signedinteger):
            flat = values.ravel()
            if flat.min() < 0:
                raise ValueError(
                    f"Negative integer values are not supported. "
                    f"Found minimum value {flat.min()}."
                )
            if flat.max() > np.iinfo(np.uint32).max:
                return MultisetCSFUint64 if multiset else CSFUint64
            return MultisetCSFUint32 if multiset else CSFUint32
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


