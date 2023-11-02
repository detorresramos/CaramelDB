from ._caramel import *


def CSF(keys, values, max_to_validate=None):
    """
    Constructs a CSF, automatically inferring the correct CSF backend.

    Arguments:
        keys: List of hashable keys.
        values: List of values to use in the CSF.
        max_to_validate: If provided, only the first "max_to_validate" values
            will be checked when inferring the correct CSF backend.
    
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

    if isinstance(values[0], int):
        return CSFUint32(keys, values)

    if isinstance(values[0], (str, bytes)):
        # call out to one of the dedicated-length strings
        max_idx = max_to_validate
        validate_values = values[:max_idx] if max_idx else values
        value_length = _infer_length(values)
        if value_length == 10:
            return CSFChar10(keys, values)
        elif value_length == 12:
            return CSFChar12(keys, values)
        else:
            return CSFString(keys, values)
    raise ValueError(f"Unsupported value type: {type(values[0])}")



def _infer_length(values):
    target_length = len(values[0])
    for v in values:
        if len(v) != target_length:
            return None
    return target_length


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
    csf_classes = [CSFUint32, CSFChar10, CSFChar12, CSFString]
    for csf_class in csf_classes:
        try:
            csf = csf_class.load(filename)
            return csf
        except CsfDeserializationException as e:
            continue
    raise ValueError(f"File {filename} does not contain a deserializable CSF.")
