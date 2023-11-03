from ._caramel import *


def CSF(keys, values, max_to_infer=None):
    """
    Constructs a CSF, automatically inferring the correct CSF backend.

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

    if isinstance(values[0], int):
        return CSFUint32(keys, values)

    if isinstance(values[0], (str, bytes)):
        # call out to one of the dedicated-length strings
        validate_values = values[:max_to_infer] if max_to_infer else values
        value_length = _infer_length(values)
        if value_length == 10:
            csf = CSFChar10(keys, values)
            return CSFQueryWrapper(csf, lambda x: ''.join(x))
        elif value_length == 12:
            csf = CSFChar12(keys, values)
            return CSFQueryWrapper(csf, lambda x: ''.join(x))
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
            if isinstance(csf, (CSFChar10, CSFChar12)):
                csf = CSFQueryWrapper(csf, lambda x: ''.join(x))
            return csf
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
        return getattr(self.__dict__['_csf'], name)
    
