# distutils: language = c++
# distutils: include_dirs = src

from libc.stdint cimport uint32_t, uint64_t
from libcpp cimport bool
from libcpp.memory cimport shared_ptr
from libcpp.string cimport string
from libcpp.utility cimport pair
from libcpp.vector cimport vector
import numpy as np
cimport numpy as np
np.import_array()

cdef extern from "src/construct/filter/FilterConfig.h" namespace "caramel":
    cdef cppclass PreFilterConfig:
        pass
    ctypedef shared_ptr[PreFilterConfig] PreFilterConfigPtr

    cdef cppclass BloomPreFilterConfig(PreFilterConfig):
        BloomPreFilterConfig() except +

cdef class PyPreFilterConfig:
    cdef PreFilterConfigPtr cpp_prefilter

    def __cinit__(self):
        self.cpp_prefilter = PreFilterConfigPtr()  # null pointer by default


cdef class PyBloomPreFilterConfig(PyPreFilterConfig):
    def __cinit__(self):
        self.cpp_prefilter = shared_ptr[PreFilterConfig](new BloomPreFilterConfig())


cdef extern from "src/construct/filter/BloomFilter.h" namespace "caramel":
    cdef cppclass BloomFilter:
        BloomFilter(size_t num_elements, float error_rate) except +
        @staticmethod 
        shared_ptr[BloomFilter] make(size_t num_elements, double error_rate)
        void add(const string& key)
        bool contains(const string& key)
        size_t size() const
        size_t numHashes() const

    ctypedef shared_ptr[BloomFilter] BloomFilterPtr


cdef class PyBloomFilter:
    cdef BloomFilterPtr cpp_bloom_filter

    def __cinit__(self, size_t num_elements, float error_rate):
        self.cpp_bloom_filter = BloomFilter.make(num_elements, error_rate)

    def add(self, key):
        if isinstance(key, str):
            key = key.encode('utf-8')
        elif not isinstance(key, bytes):
            raise TypeError("key must be str or bytes")
        self.cpp_bloom_filter.get().add(key)

    def contains(self, key):
        if isinstance(key, str):
            key = key.encode('utf-8')
        elif not isinstance(key, bytes):
            raise TypeError("key must be str or bytes")
        return self.cpp_bloom_filter.get().contains(key)

    def size(self):
        return self.cpp_bloom_filter.get().size()

    def numHashes(self):
        return self.cpp_bloom_filter.get().numHashes()


cdef extern from "src/construct/Csf.h" namespace "caramel":
    cdef cppclass CsfDeserializationException "CsfDeserializationException":
        CsfDeserializationException(const string& message)

    cdef cppclass Csf[T]:
        T query(const string &key) const
        void save(const string &filename, const uint32_t type_id) const
        @staticmethod
        shared_ptr[Csf[T]] load(const string &filename, const uint32_t type_id) except +


cdef extern from "src/construct/Construct.h" namespace "caramel":
    shared_ptr[Csf[T]] constructCsf[T](const vector[string] &keys,
                                       const vector[T] &values,
                                       PreFilterConfigPtr prefilter,
                                       bool verbose) except +


cdef extern from "src/construct/MultisetCsf.h" namespace "caramel":
    cdef cppclass MultisetCsf[T]:
        vector[T] query(const string &key, bool parallelize) const
        void save(const string &filename, const uint32_t type_id) const
        @staticmethod
        shared_ptr[MultisetCsf[T]] load(const string &filename, const uint32_t type_id) except +


cdef extern from "src/construct/ConstructMultiset.h" namespace "caramel":
    shared_ptr[MultisetCsf[T]] constructMultisetCsf[T](const vector[string] &keys,
                                       const vector[vector[T]] &values,
                                       PreFilterConfigPtr prefilter,
                                       bool verbose) except +


cdef class PyCsfUint32:
    cdef shared_ptr[Csf[uint32_t]] cpp_csf
    
    # we have a static constructor because I couldn't get the load method to 
    # work properly if this logic was defined in the __cinit__ method
    # if we can get that to work we can move this logic to the __cinit__ method
    # and remove the checks for self.cpp_csf
    @staticmethod
    def construct(vector[string] keys, vector[uint32_t] values, prefilter=None, bint verbose=True):
        cdef PreFilterConfigPtr prefilter_ptr
        if prefilter is None:
            prefilter_ptr = PreFilterConfigPtr() # nullptr by default
        elif isinstance(prefilter, PyPreFilterConfig):
            prefilter_ptr = (<PyPreFilterConfig>prefilter).cpp_prefilter
        else:
            raise TypeError("prefilter must be a PyPreFilterConfig or None")
        cdef PyCsfUint32 obj = PyCsfUint32()
        obj.cpp_csf = constructCsf(keys, values, prefilter_ptr, verbose)
        return obj

    def query(self, key):
        if not self.cpp_csf:
            raise ValueError("CSF object not built properly, please use the static construct or load methods instead.")
        return self.cpp_csf.get().query(key)

    def save(self, filename):
        if not self.cpp_csf:
            raise ValueError("CSF object not built properly, please use the static construct or load methods instead.")
        self.cpp_csf.get().save(filename, 0)

    @staticmethod
    def load(filename):
        cdef PyCsfUint32 obj = PyCsfUint32.__new__(PyCsfUint32)
        obj.cpp_csf = Csf[uint32_t].load(filename, 0)
        return obj


cdef class PyMultisetCsfUint32:
    cdef shared_ptr[MultisetCsf[uint32_t]] cpp_csf
    
    @staticmethod
    def construct(vector[string] keys, vector[vector[uint32_t]] values, prefilter=None, bint verbose=True):
        cdef PreFilterConfigPtr prefilter_ptr
        if prefilter is None:
            prefilter_ptr = PreFilterConfigPtr()
        elif isinstance(prefilter, PyPreFilterConfig):
            prefilter_ptr = (<PyPreFilterConfig>prefilter).cpp_prefilter
        else:
            raise TypeError("prefilter must be a PyPreFilterConfig or None")
        cdef PyMultisetCsfUint32 obj = PyMultisetCsfUint32()
        obj.cpp_csf = constructMultisetCsf(keys, values, prefilter_ptr, verbose)
        return obj

    def query(self, key, parallelize):
        if not self.cpp_csf:
            raise ValueError("CSF object not built properly, please use the static construct method instead.")
        return self.cpp_csf.get().query(key, parallelize)

    def save(self, filename):
        if not self.cpp_csf:
            raise ValueError("CSF object not built properly, please use the static construct method instead.")
        self.cpp_csf.get().save(filename, 0)

    @staticmethod
    def load(filename):
        cdef PyMultisetCsfUint32 obj = PyMultisetCsfUint32.__new__(PyMultisetCsfUint32)
        obj.cpp_csf = MultisetCsf[uint32_t].load(filename, 0)
        return obj

cdef extern from "<array>" namespace "std":
    cdef cppclass array_char10 "std::array<char, 10>":
        pass
    cdef cppclass array_char12 "std::array<char, 12>":
        pass

ctypedef fused PermutationType:
    uint32_t
    uint64_t
    array_char10
    array_char12

cdef extern from "src/construct/EntropyPermutation.h" namespace "caramel":
    void entropyPermutation[PermutationType](PermutationType* M, int num_rows, int num_cols) except +

def permute(np.ndarray array):
    """
    Permutes a 2D numpy array in-place using the templated C++ function.

    Supported types:
      - np.uint32 (for T = uint32_t)
      - np.uint64 (for T = uint64_t)
      - Fixed-length strings with itemsize 10 (for T = std::array<char, 10>)
      - Fixed-length strings with itemsize 12 (for T = std::array<char, 12>)
    """
    cdef int num_rows
    cdef int num_cols
    cdef uint32_t* data_uint32
    cdef uint64_t* data_uint64
    cdef array_char10* data_char10
    cdef array_char12* data_char12

    if array.ndim != 2:
        raise ValueError("Input must be a 2D numpy array")
    
    num_rows = array.shape[0]
    num_cols = array.shape[1]

    if array.dtype == np.uint32:
        data_uint32 = <uint32_t*> array.data
        entropyPermutation[uint32_t](data_uint32, num_rows, num_cols)
    elif array.dtype == np.uint64:
        data_uint64 = <uint64_t*> array.data
        entropyPermutation[uint64_t](data_uint64, num_rows, num_cols)
    elif array.dtype.kind == 'S' and array.dtype.itemsize == 10:
        data_char10 = <array_char10*> array.data
        entropyPermutation[array_char10](data_char10, num_rows, num_cols)
    elif array.dtype.kind == 'S' and array.dtype.itemsize == 12:
        data_char12 = <array_char12*> array.data
        entropyPermutation[array_char12](data_char12, num_rows, num_cols)
    else:
        raise TypeError("Unsupported dtype")


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
        warnings.filterwarnings("error", category=np.VisibleDeprecationWarning)
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