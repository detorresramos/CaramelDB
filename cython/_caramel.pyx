# distutils: language = c++
# cython: language_level = 3
# cython: always_allow_keywords = False

from cpython.bytes cimport PyBytes_AS_STRING, PyBytes_Check, PyBytes_GET_SIZE
from cpython.unicode cimport PyUnicode_Check, PyUnicode_DecodeUTF8
from libc.string cimport memcpy
from libcpp cimport bool as cbool
from libcpp.memory cimport make_shared, shared_ptr
from libcpp.string cimport string
from libcpp.vector cimport vector

cimport cython


cdef extern from "Python.h":
    const char* PyUnicode_AsUTF8AndSize(object o, Py_ssize_t *size) except NULL

cimport _caramel as cpp

import numpy as np

cimport numpy as np

np.import_array()


# ── Key encoding helper ───────────────────────────────────────────────────

cdef inline void _key_to_buf(key, const char** out_buf, Py_ssize_t* out_len) except *:
    if PyUnicode_Check(key):
        out_buf[0] = PyUnicode_AsUTF8AndSize(key, out_len)
    elif PyBytes_Check(key):
        out_buf[0] = PyBytes_AS_STRING(key)
        out_len[0] = PyBytes_GET_SIZE(key)
    else:
        raise TypeError(f"Key must be str or bytes, got {type(key).__name__}")

cdef inline string _to_cpp_string(key) except *:
    cdef const char* buf
    cdef Py_ssize_t length
    _key_to_buf(key, &buf, &length)
    return string(buf, length)

cdef vector[string] _to_cpp_strings(keys) except *:
    cdef vector[string] result
    result.reserve(len(keys))
    for k in keys:
        result.push_back(_to_cpp_string(k))
    return result



# ── Exception ──────────────────────────────────────────────────────────────

class CsfDeserializationException(Exception):
    pass


# ── Stats wrappers ─────────────────────────────────────────────────────────

cdef class FilterStats:
    cdef cpp.FilterStats _stats

    @staticmethod
    cdef FilterStats _from_cpp(cpp.FilterStats stats):
        cdef FilterStats obj = FilterStats.__new__(FilterStats)
        obj._stats = stats
        return obj

    @property
    def type(self):
        return self._stats.type.decode('utf-8')

    @property
    def size_bytes(self):
        return self._stats.size_bytes

    @property
    def num_elements(self):
        return self._stats.num_elements

    @property
    def num_hashes(self):
        if self._stats.num_hashes.has_value():
            return self._stats.num_hashes.value()
        return None

    @property
    def size_bits(self):
        if self._stats.size_bits.has_value():
            return self._stats.size_bits.value()
        return None

    @property
    def fingerprint_bits(self):
        if self._stats.fingerprint_bits.has_value():
            return self._stats.fingerprint_bits.value()
        return None


cdef class HuffmanStats:
    cdef cpp.HuffmanStats _stats

    @staticmethod
    cdef HuffmanStats _from_cpp(cpp.HuffmanStats stats):
        cdef HuffmanStats obj = HuffmanStats.__new__(HuffmanStats)
        obj._stats = stats
        return obj

    @property
    def num_unique_symbols(self):
        return self._stats.num_unique_symbols

    @property
    def max_code_length(self):
        return self._stats.max_code_length

    @property
    def avg_bits_per_symbol(self):
        return self._stats.avg_bits_per_symbol

    @property
    def code_length_distribution(self):
        return list(self._stats.code_length_distribution)


cdef class BucketStats:
    cdef cpp.BucketStats _stats

    @staticmethod
    cdef BucketStats _from_cpp(cpp.BucketStats stats):
        cdef BucketStats obj = BucketStats.__new__(BucketStats)
        obj._stats = stats
        return obj

    @property
    def num_buckets(self):
        return self._stats.num_buckets

    @property
    def total_solution_bits(self):
        return self._stats.total_solution_bits

    @property
    def avg_solution_bits(self):
        return self._stats.avg_solution_bits

    @property
    def min_solution_bits(self):
        return self._stats.min_solution_bits

    @property
    def max_solution_bits(self):
        return self._stats.max_solution_bits


cdef class CsfStats:
    cdef cpp.CsfStats _stats

    @staticmethod
    cdef CsfStats _from_cpp(cpp.CsfStats stats):
        cdef CsfStats obj = CsfStats.__new__(CsfStats)
        obj._stats = stats
        return obj

    @property
    def in_memory_bytes(self):
        return self._stats.in_memory_bytes

    @property
    def bucket_stats(self):
        return BucketStats._from_cpp(self._stats.bucket_stats)

    @property
    def huffman_stats(self):
        return HuffmanStats._from_cpp(self._stats.huffman_stats)

    @property
    def filter_stats(self):
        if self._stats.filter_stats.has_value():
            return FilterStats._from_cpp(self._stats.filter_stats.value())
        return None

    @property
    def solution_bytes(self):
        return self._stats.solution_bytes

    @property
    def filter_bytes(self):
        return self._stats.filter_bytes

    @property
    def metadata_bytes(self):
        return self._stats.metadata_bytes


# ── Filter Config ──────────────────────────────────────────────────────────

cdef class PreFilterConfig:
    cdef shared_ptr[cpp.PreFilterConfig] _ptr

cdef class BloomFilterConfig(PreFilterConfig):
    cdef size_t _bits_per_element
    cdef size_t _num_hashes

    def __init__(self, size_t bits_per_element, size_t num_hashes):
        self._ptr = <shared_ptr[cpp.PreFilterConfig]>make_shared[cpp.BloomPreFilterConfig](bits_per_element, num_hashes)
        self._bits_per_element = bits_per_element
        self._num_hashes = num_hashes

    @property
    def bits_per_element(self):
        return self._bits_per_element

    @bits_per_element.setter
    def bits_per_element(self, size_t value):
        self._bits_per_element = value

    @property
    def num_hashes(self):
        return self._num_hashes

    @num_hashes.setter
    def num_hashes(self, size_t value):
        self._num_hashes = value

cdef class XORFilterConfig(PreFilterConfig):
    cdef int _fingerprint_bits

    def __init__(self, int fingerprint_bits):
        self._ptr = <shared_ptr[cpp.PreFilterConfig]>make_shared[cpp.XORPreFilterConfig](fingerprint_bits)
        self._fingerprint_bits = fingerprint_bits

    @property
    def fingerprint_bits(self):
        return self._fingerprint_bits

    @fingerprint_bits.setter
    def fingerprint_bits(self, int value):
        self._fingerprint_bits = value

cdef class BinaryFuseFilterConfig(PreFilterConfig):
    cdef int _fingerprint_bits

    def __init__(self, int fingerprint_bits):
        self._ptr = <shared_ptr[cpp.PreFilterConfig]>make_shared[cpp.BinaryFusePreFilterConfig](fingerprint_bits)
        self._fingerprint_bits = fingerprint_bits

    @property
    def fingerprint_bits(self):
        return self._fingerprint_bits

    @fingerprint_bits.setter
    def fingerprint_bits(self, int value):
        self._fingerprint_bits = value

cdef class AutoFilterConfig(PreFilterConfig):
    """Automatically selects optimal filter type and parameters."""
    def __init__(self):
        self._ptr = <shared_ptr[cpp.PreFilterConfig]>make_shared[cpp.AutoPreFilterConfig]()


# ── PermutationConfig wrappers ─────────────────────────────────────────────

cdef class PermutationConfig:
    cdef shared_ptr[cpp.PermutationConfig] _ptr

cdef class EntropyPermutationConfig(PermutationConfig):
    def __init__(self):
        self._ptr = <shared_ptr[cpp.PermutationConfig]>make_shared[cpp.EntropyPermutationConfig]()

cdef class GlobalSortPermutationConfig(PermutationConfig):
    cdef int _refinement_iterations

    def __init__(self, int refinement_iterations=5):
        self._ptr = <shared_ptr[cpp.PermutationConfig]>make_shared[cpp.GlobalSortPermutationConfig](refinement_iterations)
        self._refinement_iterations = refinement_iterations

    @property
    def refinement_iterations(self):
        return self._refinement_iterations


# ── PreFilter wrappers ─────────────────────────────────────────────────────

cdef class PreFilterUint32:
    cdef shared_ptr[cpp.PreFilter_uint32] _ptr

    @staticmethod
    cdef PreFilterUint32 _wrap(shared_ptr[cpp.PreFilter_uint32] ptr):
        if not ptr:
            return None
        cdef PreFilterUint32 obj = PreFilterUint32.__new__(PreFilterUint32)
        obj._ptr = ptr
        return obj

    def contains(self, key):
        return self._ptr.get().contains(_to_cpp_string(key))

    def get_most_common_value(self):
        cdef cpp.optional[unsigned int] val = self._ptr.get().getMostCommonValue()
        if val.has_value():
            return val.value()
        return None

    def get_stats(self):
        cdef cpp.optional[cpp.FilterStats] stats = self._ptr.get().getStats()
        if stats.has_value():
            return FilterStats._from_cpp(stats.value())
        return None

    def save(self, str filename):
        cpp.savePreFilter_uint32(self._ptr, filename.encode('utf-8'))

cdef class PreFilterUint64:
    cdef shared_ptr[cpp.PreFilter_uint64] _ptr

    @staticmethod
    cdef PreFilterUint64 _wrap(shared_ptr[cpp.PreFilter_uint64] ptr):
        if not ptr:
            return None
        cdef PreFilterUint64 obj = PreFilterUint64.__new__(PreFilterUint64)
        obj._ptr = ptr
        return obj

    def contains(self, key):
        return self._ptr.get().contains(_to_cpp_string(key))

    def get_most_common_value(self):
        cdef cpp.optional[unsigned long long] val = self._ptr.get().getMostCommonValue()
        if val.has_value():
            return val.value()
        return None

    def get_stats(self):
        cdef cpp.optional[cpp.FilterStats] stats = self._ptr.get().getStats()
        if stats.has_value():
            return FilterStats._from_cpp(stats.value())
        return None

    def save(self, str filename):
        cpp.savePreFilter_uint64(self._ptr, filename.encode('utf-8'))

cdef class PreFilterChar10:
    cdef shared_ptr[cpp.PreFilter_Char10] _ptr

    @staticmethod
    cdef PreFilterChar10 _wrap(shared_ptr[cpp.PreFilter_Char10] ptr):
        if not ptr:
            return None
        cdef PreFilterChar10 obj = PreFilterChar10.__new__(PreFilterChar10)
        obj._ptr = ptr
        return obj

    def contains(self, key):
        return self._ptr.get().contains(_to_cpp_string(key))

    def get_most_common_value(self):
        cdef cpp.optional[cpp.Char10] val = self._ptr.get().getMostCommonValue()
        if val.has_value():
            return _char10_to_str(val.value())
        return None

    def get_stats(self):
        cdef cpp.optional[cpp.FilterStats] stats = self._ptr.get().getStats()
        if stats.has_value():
            return FilterStats._from_cpp(stats.value())
        return None

    def save(self, str filename):
        cpp.savePreFilter_Char10(self._ptr, filename.encode('utf-8'))

cdef class PreFilterChar12:
    cdef shared_ptr[cpp.PreFilter_Char12] _ptr

    @staticmethod
    cdef PreFilterChar12 _wrap(shared_ptr[cpp.PreFilter_Char12] ptr):
        if not ptr:
            return None
        cdef PreFilterChar12 obj = PreFilterChar12.__new__(PreFilterChar12)
        obj._ptr = ptr
        return obj

    def contains(self, key):
        return self._ptr.get().contains(_to_cpp_string(key))

    def get_most_common_value(self):
        cdef cpp.optional[cpp.Char12] val = self._ptr.get().getMostCommonValue()
        if val.has_value():
            return _char12_to_str(val.value())
        return None

    def get_stats(self):
        cdef cpp.optional[cpp.FilterStats] stats = self._ptr.get().getStats()
        if stats.has_value():
            return FilterStats._from_cpp(stats.value())
        return None

    def save(self, str filename):
        cpp.savePreFilter_Char12(self._ptr, filename.encode('utf-8'))

cdef class PreFilterString:
    cdef shared_ptr[cpp.PreFilter_string] _ptr

    @staticmethod
    cdef PreFilterString _wrap(shared_ptr[cpp.PreFilter_string] ptr):
        if not ptr:
            return None
        cdef PreFilterString obj = PreFilterString.__new__(PreFilterString)
        obj._ptr = ptr
        return obj

    def contains(self, key):
        return self._ptr.get().contains(_to_cpp_string(key))

    def get_most_common_value(self):
        cdef cpp.optional[string] val = self._ptr.get().getMostCommonValue()
        if val.has_value():
            return val.value().decode('utf-8')
        return None

    def get_stats(self):
        cdef cpp.optional[cpp.FilterStats] stats = self._ptr.get().getStats()
        if stats.has_value():
            return FilterStats._from_cpp(stats.value())
        return None

    def save(self, str filename):
        cpp.savePreFilter_string(self._ptr, filename.encode('utf-8'))


# ── PreFilter subclass wrappers (Bloom) ────────────────────────────────────

cdef class BloomPreFilterUint32(PreFilterUint32):
    cdef shared_ptr[cpp.BloomPreFilter_uint32] _sub_ptr

    def save(self, str filename):
        self._sub_ptr.get().save(filename.encode('utf-8'))

    @staticmethod
    def load(str filename):
        cdef shared_ptr[cpp.BloomPreFilter_uint32] ptr = cpp.BloomPreFilter_uint32.load(filename.encode('utf-8'))
        cdef BloomPreFilterUint32 obj = BloomPreFilterUint32.__new__(BloomPreFilterUint32)
        obj._sub_ptr = ptr
        obj._ptr = <shared_ptr[cpp.PreFilter_uint32]>ptr
        return obj

cdef class BloomPreFilterUint64(PreFilterUint64):
    cdef shared_ptr[cpp.BloomPreFilter_uint64] _sub_ptr

    def save(self, str filename):
        self._sub_ptr.get().save(filename.encode('utf-8'))

    @staticmethod
    def load(str filename):
        cdef shared_ptr[cpp.BloomPreFilter_uint64] ptr = cpp.BloomPreFilter_uint64.load(filename.encode('utf-8'))
        cdef BloomPreFilterUint64 obj = BloomPreFilterUint64.__new__(BloomPreFilterUint64)
        obj._sub_ptr = ptr
        obj._ptr = <shared_ptr[cpp.PreFilter_uint64]>ptr
        return obj

cdef class BloomPreFilterChar10(PreFilterChar10):
    cdef shared_ptr[cpp.BloomPreFilter_Char10] _sub_ptr

    def save(self, str filename):
        self._sub_ptr.get().save(filename.encode('utf-8'))

    @staticmethod
    def load(str filename):
        cdef shared_ptr[cpp.BloomPreFilter_Char10] ptr = cpp.BloomPreFilter_Char10.load(filename.encode('utf-8'))
        cdef BloomPreFilterChar10 obj = BloomPreFilterChar10.__new__(BloomPreFilterChar10)
        obj._sub_ptr = ptr
        obj._ptr = <shared_ptr[cpp.PreFilter_Char10]>ptr
        return obj

cdef class BloomPreFilterChar12(PreFilterChar12):
    cdef shared_ptr[cpp.BloomPreFilter_Char12] _sub_ptr

    def save(self, str filename):
        self._sub_ptr.get().save(filename.encode('utf-8'))

    @staticmethod
    def load(str filename):
        cdef shared_ptr[cpp.BloomPreFilter_Char12] ptr = cpp.BloomPreFilter_Char12.load(filename.encode('utf-8'))
        cdef BloomPreFilterChar12 obj = BloomPreFilterChar12.__new__(BloomPreFilterChar12)
        obj._sub_ptr = ptr
        obj._ptr = <shared_ptr[cpp.PreFilter_Char12]>ptr
        return obj

cdef class BloomPreFilterString(PreFilterString):
    cdef shared_ptr[cpp.BloomPreFilter_string] _sub_ptr

    def save(self, str filename):
        self._sub_ptr.get().save(filename.encode('utf-8'))

    @staticmethod
    def load(str filename):
        cdef shared_ptr[cpp.BloomPreFilter_string] ptr = cpp.BloomPreFilter_string.load(filename.encode('utf-8'))
        cdef BloomPreFilterString obj = BloomPreFilterString.__new__(BloomPreFilterString)
        obj._sub_ptr = ptr
        obj._ptr = <shared_ptr[cpp.PreFilter_string]>ptr
        return obj


# ── PreFilter subclass wrappers (XOR) ─────────────────────────────────────

cdef class XORPreFilterUint32(PreFilterUint32):
    cdef shared_ptr[cpp.XORPreFilter_uint32] _sub_ptr

    def save(self, str filename):
        self._sub_ptr.get().save(filename.encode('utf-8'))

    @staticmethod
    def load(str filename):
        cdef shared_ptr[cpp.XORPreFilter_uint32] ptr = cpp.XORPreFilter_uint32.load(filename.encode('utf-8'))
        cdef XORPreFilterUint32 obj = XORPreFilterUint32.__new__(XORPreFilterUint32)
        obj._sub_ptr = ptr
        obj._ptr = <shared_ptr[cpp.PreFilter_uint32]>ptr
        return obj

cdef class XORPreFilterUint64(PreFilterUint64):
    cdef shared_ptr[cpp.XORPreFilter_uint64] _sub_ptr

    def save(self, str filename):
        self._sub_ptr.get().save(filename.encode('utf-8'))

    @staticmethod
    def load(str filename):
        cdef shared_ptr[cpp.XORPreFilter_uint64] ptr = cpp.XORPreFilter_uint64.load(filename.encode('utf-8'))
        cdef XORPreFilterUint64 obj = XORPreFilterUint64.__new__(XORPreFilterUint64)
        obj._sub_ptr = ptr
        obj._ptr = <shared_ptr[cpp.PreFilter_uint64]>ptr
        return obj

cdef class XORPreFilterChar10(PreFilterChar10):
    cdef shared_ptr[cpp.XORPreFilter_Char10] _sub_ptr

    def save(self, str filename):
        self._sub_ptr.get().save(filename.encode('utf-8'))

    @staticmethod
    def load(str filename):
        cdef shared_ptr[cpp.XORPreFilter_Char10] ptr = cpp.XORPreFilter_Char10.load(filename.encode('utf-8'))
        cdef XORPreFilterChar10 obj = XORPreFilterChar10.__new__(XORPreFilterChar10)
        obj._sub_ptr = ptr
        obj._ptr = <shared_ptr[cpp.PreFilter_Char10]>ptr
        return obj

cdef class XORPreFilterChar12(PreFilterChar12):
    cdef shared_ptr[cpp.XORPreFilter_Char12] _sub_ptr

    def save(self, str filename):
        self._sub_ptr.get().save(filename.encode('utf-8'))

    @staticmethod
    def load(str filename):
        cdef shared_ptr[cpp.XORPreFilter_Char12] ptr = cpp.XORPreFilter_Char12.load(filename.encode('utf-8'))
        cdef XORPreFilterChar12 obj = XORPreFilterChar12.__new__(XORPreFilterChar12)
        obj._sub_ptr = ptr
        obj._ptr = <shared_ptr[cpp.PreFilter_Char12]>ptr
        return obj

cdef class XORPreFilterString(PreFilterString):
    cdef shared_ptr[cpp.XORPreFilter_string] _sub_ptr

    def save(self, str filename):
        self._sub_ptr.get().save(filename.encode('utf-8'))

    @staticmethod
    def load(str filename):
        cdef shared_ptr[cpp.XORPreFilter_string] ptr = cpp.XORPreFilter_string.load(filename.encode('utf-8'))
        cdef XORPreFilterString obj = XORPreFilterString.__new__(XORPreFilterString)
        obj._sub_ptr = ptr
        obj._ptr = <shared_ptr[cpp.PreFilter_string]>ptr
        return obj


# ── PreFilter subclass wrappers (BinaryFuse) ──────────────────────────────

cdef class BinaryFusePreFilterUint32(PreFilterUint32):
    cdef shared_ptr[cpp.BinaryFusePreFilter_uint32] _sub_ptr

    def save(self, str filename):
        self._sub_ptr.get().save(filename.encode('utf-8'))

    @staticmethod
    def load(str filename):
        cdef shared_ptr[cpp.BinaryFusePreFilter_uint32] ptr = cpp.BinaryFusePreFilter_uint32.load(filename.encode('utf-8'))
        cdef BinaryFusePreFilterUint32 obj = BinaryFusePreFilterUint32.__new__(BinaryFusePreFilterUint32)
        obj._sub_ptr = ptr
        obj._ptr = <shared_ptr[cpp.PreFilter_uint32]>ptr
        return obj

cdef class BinaryFusePreFilterUint64(PreFilterUint64):
    cdef shared_ptr[cpp.BinaryFusePreFilter_uint64] _sub_ptr

    def save(self, str filename):
        self._sub_ptr.get().save(filename.encode('utf-8'))

    @staticmethod
    def load(str filename):
        cdef shared_ptr[cpp.BinaryFusePreFilter_uint64] ptr = cpp.BinaryFusePreFilter_uint64.load(filename.encode('utf-8'))
        cdef BinaryFusePreFilterUint64 obj = BinaryFusePreFilterUint64.__new__(BinaryFusePreFilterUint64)
        obj._sub_ptr = ptr
        obj._ptr = <shared_ptr[cpp.PreFilter_uint64]>ptr
        return obj

cdef class BinaryFusePreFilterChar10(PreFilterChar10):
    cdef shared_ptr[cpp.BinaryFusePreFilter_Char10] _sub_ptr

    def save(self, str filename):
        self._sub_ptr.get().save(filename.encode('utf-8'))

    @staticmethod
    def load(str filename):
        cdef shared_ptr[cpp.BinaryFusePreFilter_Char10] ptr = cpp.BinaryFusePreFilter_Char10.load(filename.encode('utf-8'))
        cdef BinaryFusePreFilterChar10 obj = BinaryFusePreFilterChar10.__new__(BinaryFusePreFilterChar10)
        obj._sub_ptr = ptr
        obj._ptr = <shared_ptr[cpp.PreFilter_Char10]>ptr
        return obj

cdef class BinaryFusePreFilterChar12(PreFilterChar12):
    cdef shared_ptr[cpp.BinaryFusePreFilter_Char12] _sub_ptr

    def save(self, str filename):
        self._sub_ptr.get().save(filename.encode('utf-8'))

    @staticmethod
    def load(str filename):
        cdef shared_ptr[cpp.BinaryFusePreFilter_Char12] ptr = cpp.BinaryFusePreFilter_Char12.load(filename.encode('utf-8'))
        cdef BinaryFusePreFilterChar12 obj = BinaryFusePreFilterChar12.__new__(BinaryFusePreFilterChar12)
        obj._sub_ptr = ptr
        obj._ptr = <shared_ptr[cpp.PreFilter_Char12]>ptr
        return obj

cdef class BinaryFusePreFilterString(PreFilterString):
    cdef shared_ptr[cpp.BinaryFusePreFilter_string] _sub_ptr

    def save(self, str filename):
        self._sub_ptr.get().save(filename.encode('utf-8'))

    @staticmethod
    def load(str filename):
        cdef shared_ptr[cpp.BinaryFusePreFilter_string] ptr = cpp.BinaryFusePreFilter_string.load(filename.encode('utf-8'))
        cdef BinaryFusePreFilterString obj = BinaryFusePreFilterString.__new__(BinaryFusePreFilterString)
        obj._sub_ptr = ptr
        obj._ptr = <shared_ptr[cpp.PreFilter_string]>ptr
        return obj


# ── BloomFilter (standalone) ───────────────────────────────────────────────

cdef class BloomFilter:
    cdef shared_ptr[cpp.BloomFilter] _ptr

    @staticmethod
    def autotuned(size_t num_elements, double error_rate, bint verbose=False):
        cdef BloomFilter obj = BloomFilter.__new__(BloomFilter)
        obj._ptr = cpp.BloomFilter.makeAutotuned(num_elements, error_rate, verbose)
        return obj

    @staticmethod
    def fixed(size_t bitarray_size, size_t num_hashes):
        cdef BloomFilter obj = BloomFilter.__new__(BloomFilter)
        obj._ptr = cpp.BloomFilter.makeFixed(bitarray_size, num_hashes)
        return obj

    def add(self, key):
        self._ptr.get().add(_to_cpp_string(key))

    def size(self):
        return self._ptr.get().size()

    def num_hashes(self):
        return self._ptr.get().numHashes()

    def contains(self, key):
        return self._ptr.get().contains(_to_cpp_string(key))


# ── Char10/Char12 conversion helpers ──────────────────────────────────────

cdef cpp.Char10 _str_to_char10(s) except *:
    cdef cpp.Char10 result
    cdef const char* buf
    cdef Py_ssize_t slen
    if PyUnicode_Check(s):
        buf = PyUnicode_AsUTF8AndSize(s, &slen)
    elif PyBytes_Check(s):
        buf = PyBytes_AS_STRING(s)
        slen = PyBytes_GET_SIZE(s)
    else:
        raise TypeError(f"Expected str or bytes, got {type(s).__name__}")
    if slen != 10:
        raise ValueError(f"Expected 10-byte value, got {slen}")
    memcpy(result.data(), buf, 10)
    return result

cdef cpp.Char12 _str_to_char12(s) except *:
    cdef cpp.Char12 result
    cdef const char* buf
    cdef Py_ssize_t slen
    if PyUnicode_Check(s):
        buf = PyUnicode_AsUTF8AndSize(s, &slen)
    elif PyBytes_Check(s):
        buf = PyBytes_AS_STRING(s)
        slen = PyBytes_GET_SIZE(s)
    else:
        raise TypeError(f"Expected str or bytes, got {type(s).__name__}")
    if slen != 12:
        raise ValueError(f"Expected 12-byte value, got {slen}")
    memcpy(result.data(), buf, 12)
    return result

cdef inline str _char10_to_str(cpp.Char10 c):
    return PyUnicode_DecodeUTF8(c.data(), 10, NULL)

cdef inline str _char12_to_str(cpp.Char12 c):
    return PyUnicode_DecodeUTF8(c.data(), 12, NULL)


# ── CSF wrappers ───────────────────────────────────────────────────────────

cdef class CSFUint32:
    cdef shared_ptr[cpp.Csf_uint32] _ptr

    def __init__(self, list keys, values, prefilter=None, bint verbose=True):
        cdef vector[string] cpp_keys = _to_cpp_strings(keys)
        cdef vector[unsigned int] cpp_values
        cdef shared_ptr[cpp.PreFilterConfig] filter_config
        cdef Py_ssize_t n = len(values)

        if isinstance(values, np.ndarray):
            arr_u32 = np.ascontiguousarray(values, dtype=np.uint32)
            cpp_values.resize(n)
            memcpy(cpp_values.data(), (<np.ndarray>arr_u32).data, n * sizeof(unsigned int))
        else:
            cpp_values.reserve(n)
            for v in values:
                cpp_values.push_back(<unsigned int>v)

        if prefilter is not None and isinstance(prefilter, PreFilterConfig):
            filter_config = (<PreFilterConfig>prefilter)._ptr

        self._ptr = cpp.constructCsf_uint32(cpp_keys, cpp_values, filter_config, verbose)

    def query(self, key):
        cdef const char* buf
        cdef Py_ssize_t length
        _key_to_buf(key, &buf, &length)
        return self._ptr.get().query(buf, length)

    def query_batch(self, list keys):
        cdef Py_ssize_t n = len(keys)
        cdef np.ndarray[np.uint32_t, ndim=1] results = np.empty(n, dtype=np.uint32)
        cdef unsigned int* out = <unsigned int*>results.data
        cdef const char* buf
        cdef Py_ssize_t length
        for i in range(n):
            _key_to_buf(keys[i], &buf, &length)
            out[i] = self._ptr.get().query(buf, length)
        return results

    def get_filter(self):
        return PreFilterUint32._wrap(self._ptr.get().getFilter())

    def get_stats(self):
        return CsfStats._from_cpp(self._ptr.get().getStats())

    def save(self, str filename):
        self._ptr.get().save(filename.encode('utf-8'), 1)

    @staticmethod
    def load(str filename):
        cdef CSFUint32 obj = CSFUint32.__new__(CSFUint32)
        try:
            obj._ptr = cpp.Csf_uint32.load(filename.encode('utf-8'), 1)
        except RuntimeError as e:
            raise CsfDeserializationException(str(e))
        return obj

    @staticmethod
    def is_multiset():
        return False


cdef class CSFUint64:
    cdef shared_ptr[cpp.Csf_uint64] _ptr

    def __init__(self, list keys, values, prefilter=None, bint verbose=True):
        cdef vector[string] cpp_keys = _to_cpp_strings(keys)
        cdef vector[unsigned long long] cpp_values
        cdef shared_ptr[cpp.PreFilterConfig] filter_config
        cdef Py_ssize_t n = len(values)

        if isinstance(values, np.ndarray):
            arr_u64 = np.ascontiguousarray(values, dtype=np.uint64)
            cpp_values.resize(n)
            memcpy(cpp_values.data(), (<np.ndarray>arr_u64).data, n * sizeof(unsigned long long))
        else:
            cpp_values.reserve(n)
            for v in values:
                cpp_values.push_back(<unsigned long long>v)

        if prefilter is not None and isinstance(prefilter, PreFilterConfig):
            filter_config = (<PreFilterConfig>prefilter)._ptr

        self._ptr = cpp.constructCsf_uint64(cpp_keys, cpp_values, filter_config, verbose)

    def query(self, key):
        cdef const char* buf
        cdef Py_ssize_t length
        _key_to_buf(key, &buf, &length)
        return self._ptr.get().query(buf, length)

    def query_batch(self, list keys):
        cdef Py_ssize_t n = len(keys)
        cdef np.ndarray[np.uint64_t, ndim=1] results = np.empty(n, dtype=np.uint64)
        cdef unsigned long long* out = <unsigned long long*>results.data
        cdef const char* buf
        cdef Py_ssize_t length
        for i in range(n):
            _key_to_buf(keys[i], &buf, &length)
            out[i] = self._ptr.get().query(buf, length)
        return results

    def get_filter(self):
        return PreFilterUint64._wrap(self._ptr.get().getFilter())

    def get_stats(self):
        return CsfStats._from_cpp(self._ptr.get().getStats())

    def save(self, str filename):
        self._ptr.get().save(filename.encode('utf-8'), 2)

    @staticmethod
    def load(str filename):
        cdef CSFUint64 obj = CSFUint64.__new__(CSFUint64)
        try:
            obj._ptr = cpp.Csf_uint64.load(filename.encode('utf-8'), 2)
        except RuntimeError as e:
            raise CsfDeserializationException(str(e))
        return obj

    @staticmethod
    def is_multiset():
        return False


cdef class CSFChar10:
    cdef shared_ptr[cpp.Csf_Char10] _ptr

    def __init__(self, list keys, values, prefilter=None, bint verbose=True):
        cdef vector[string] cpp_keys = _to_cpp_strings(keys)
        cdef vector[cpp.Char10] cpp_values
        cdef shared_ptr[cpp.PreFilterConfig] filter_config

        cpp_values.reserve(len(values))
        for v in values:
            cpp_values.push_back(_str_to_char10(v))

        if prefilter is not None and isinstance(prefilter, PreFilterConfig):
            filter_config = (<PreFilterConfig>prefilter)._ptr

        self._ptr = cpp.constructCsf_Char10(cpp_keys, cpp_values, filter_config, verbose)

    def query(self, key):
        cdef const char* buf
        cdef Py_ssize_t length
        _key_to_buf(key, &buf, &length)
        cdef cpp.Char10 result = self._ptr.get().query(buf, length)
        return _char10_to_str(result)

    def query_batch(self, list keys):
        cdef Py_ssize_t n = len(keys)
        cdef const char* buf
        cdef Py_ssize_t length
        cdef cpp.Char10 r
        py_results = []
        for i in range(n):
            _key_to_buf(keys[i], &buf, &length)
            r = self._ptr.get().query(buf, length)
            py_results.append(_char10_to_str(r))
        return py_results

    def get_filter(self):
        return PreFilterChar10._wrap(self._ptr.get().getFilter())

    def get_stats(self):
        return CsfStats._from_cpp(self._ptr.get().getStats())

    def save(self, str filename):
        self._ptr.get().save(filename.encode('utf-8'), 3)

    @staticmethod
    def load(str filename):
        cdef CSFChar10 obj = CSFChar10.__new__(CSFChar10)
        try:
            obj._ptr = cpp.Csf_Char10.load(filename.encode('utf-8'), 3)
        except RuntimeError as e:
            raise CsfDeserializationException(str(e))
        return obj

    @staticmethod
    def is_multiset():
        return False


cdef class CSFChar12:
    cdef shared_ptr[cpp.Csf_Char12] _ptr

    def __init__(self, list keys, values, prefilter=None, bint verbose=True):
        cdef vector[string] cpp_keys = _to_cpp_strings(keys)
        cdef vector[cpp.Char12] cpp_values
        cdef shared_ptr[cpp.PreFilterConfig] filter_config

        cpp_values.reserve(len(values))
        for v in values:
            cpp_values.push_back(_str_to_char12(v))

        if prefilter is not None and isinstance(prefilter, PreFilterConfig):
            filter_config = (<PreFilterConfig>prefilter)._ptr

        self._ptr = cpp.constructCsf_Char12(cpp_keys, cpp_values, filter_config, verbose)

    def query(self, key):
        cdef const char* buf
        cdef Py_ssize_t length
        _key_to_buf(key, &buf, &length)
        cdef cpp.Char12 result = self._ptr.get().query(buf, length)
        return _char12_to_str(result)

    def query_batch(self, list keys):
        cdef Py_ssize_t n = len(keys)
        cdef const char* buf
        cdef Py_ssize_t length
        cdef cpp.Char12 r
        py_results = []
        for i in range(n):
            _key_to_buf(keys[i], &buf, &length)
            r = self._ptr.get().query(buf, length)
            py_results.append(_char12_to_str(r))
        return py_results

    def get_filter(self):
        return PreFilterChar12._wrap(self._ptr.get().getFilter())

    def get_stats(self):
        return CsfStats._from_cpp(self._ptr.get().getStats())

    def save(self, str filename):
        self._ptr.get().save(filename.encode('utf-8'), 4)

    @staticmethod
    def load(str filename):
        cdef CSFChar12 obj = CSFChar12.__new__(CSFChar12)
        try:
            obj._ptr = cpp.Csf_Char12.load(filename.encode('utf-8'), 4)
        except RuntimeError as e:
            raise CsfDeserializationException(str(e))
        return obj

    @staticmethod
    def is_multiset():
        return False


cdef class CSFString:
    cdef shared_ptr[cpp.Csf_string] _ptr

    def __init__(self, list keys, values, prefilter=None, bint verbose=True):
        cdef vector[string] cpp_keys = _to_cpp_strings(keys)
        cdef vector[string] cpp_values
        cdef shared_ptr[cpp.PreFilterConfig] filter_config

        cpp_values.reserve(len(values))
        for v in values:
            if isinstance(v, str):
                cpp_values.push_back((<str>v).encode('utf-8'))
            else:
                cpp_values.push_back(<bytes>v)

        if prefilter is not None and isinstance(prefilter, PreFilterConfig):
            filter_config = (<PreFilterConfig>prefilter)._ptr

        self._ptr = cpp.constructCsf_string(cpp_keys, cpp_values, filter_config, verbose)

    def query(self, key):
        cdef const char* buf
        cdef Py_ssize_t length
        _key_to_buf(key, &buf, &length)
        cdef string result = self._ptr.get().query(buf, length)
        return result.decode('utf-8')

    def query_batch(self, list keys):
        cdef Py_ssize_t n = len(keys)
        cdef const char* buf
        cdef Py_ssize_t length
        cdef string r
        py_results = []
        for i in range(n):
            _key_to_buf(keys[i], &buf, &length)
            r = self._ptr.get().query(buf, length)
            py_results.append(r.decode('utf-8'))
        return py_results

    def get_filter(self):
        return PreFilterString._wrap(self._ptr.get().getFilter())

    def get_stats(self):
        return CsfStats._from_cpp(self._ptr.get().getStats())

    def save(self, str filename):
        self._ptr.get().save(filename.encode('utf-8'), 5)

    @staticmethod
    def load(str filename):
        cdef CSFString obj = CSFString.__new__(CSFString)
        try:
            obj._ptr = cpp.Csf_string.load(filename.encode('utf-8'), 5)
        except RuntimeError as e:
            raise CsfDeserializationException(str(e))
        return obj

    @staticmethod
    def is_multiset():
        return False


# ── MultisetCSF wrappers ──────────────────────────────────────────────────

cdef class MultisetCSFUint32:
    cdef shared_ptr[cpp.MultisetCsf_uint32] _ptr
    cdef public double permutation_seconds
    cdef public double build_seconds

    def __init__(self, list keys, values, prefilter=None, permutation=None,
                 bint shared_codebook=False, bint shared_filter=False, bint verbose=True):
        cdef vector[string] cpp_keys = _to_cpp_strings(keys)

        values = np.ascontiguousarray(values, dtype=np.uint32)
        if values.ndim != 2:
            raise ValueError("MultisetCSF values must be a 2D array.")

        if permutation is not None:
            values = values.copy()

        cdef int num_rows = values.shape[0]
        cdef int num_cols = values.shape[1]

        cdef cpp.MultisetConfig config
        if permutation is not None and isinstance(permutation, PermutationConfig):
            config.permutation_config = (<PermutationConfig>permutation)._ptr
        config.verbose = verbose
        config.shared_codebook = shared_codebook
        config.shared_filter = shared_filter
        if prefilter is not None and isinstance(prefilter, PreFilterConfig):
            config.filter_config = (<PreFilterConfig>prefilter)._ptr

        cdef cpp.MultisetConstructionResult_uint32 result
        result = cpp.constructMultisetCsfRowMajor_uint32(
            cpp_keys, <unsigned int*>(<np.ndarray>values).data,
            num_rows, num_cols, config)
        self._ptr = result.csf
        self.permutation_seconds = result.permutation_seconds
        self.build_seconds = result.build_seconds

    def query(self, key, bint parallel=False):
        cdef const char* buf
        cdef Py_ssize_t length
        _key_to_buf(key, &buf, &length)
        return list(self._ptr.get().query(buf, length, parallel))

    def save(self, str filename):
        self._ptr.get().save(filename.encode('utf-8'), 101)

    @staticmethod
    def load(str filename):
        cdef MultisetCSFUint32 obj = MultisetCSFUint32.__new__(MultisetCSFUint32)
        try:
            obj._ptr = cpp.MultisetCsf_uint32.load(filename.encode('utf-8'), 101)
        except RuntimeError as e:
            raise CsfDeserializationException(str(e))
        obj.permutation_seconds = 0.0
        obj.build_seconds = 0.0
        return obj

    @staticmethod
    def is_multiset():
        return True


cdef class MultisetCSFUint64:
    cdef shared_ptr[cpp.MultisetCsf_uint64] _ptr
    cdef public double permutation_seconds
    cdef public double build_seconds

    def __init__(self, list keys, values, prefilter=None, permutation=None,
                 bint shared_codebook=False, bint shared_filter=False, bint verbose=True):
        cdef vector[string] cpp_keys = _to_cpp_strings(keys)

        values = np.ascontiguousarray(values, dtype=np.uint64)
        if values.ndim != 2:
            raise ValueError("MultisetCSF values must be a 2D array.")

        if permutation is not None:
            values = values.copy()

        cdef int num_rows = values.shape[0]
        cdef int num_cols = values.shape[1]

        cdef cpp.MultisetConfig config
        if permutation is not None and isinstance(permutation, PermutationConfig):
            config.permutation_config = (<PermutationConfig>permutation)._ptr
        config.verbose = verbose
        config.shared_codebook = shared_codebook
        config.shared_filter = shared_filter
        if prefilter is not None and isinstance(prefilter, PreFilterConfig):
            config.filter_config = (<PreFilterConfig>prefilter)._ptr

        cdef cpp.MultisetConstructionResult_uint64 result
        result = cpp.constructMultisetCsfRowMajor_uint64(
            cpp_keys, <unsigned long long*>(<np.ndarray>values).data,
            num_rows, num_cols, config)
        self._ptr = result.csf
        self.permutation_seconds = result.permutation_seconds
        self.build_seconds = result.build_seconds

    def query(self, key, bint parallel=False):
        cdef const char* buf
        cdef Py_ssize_t length
        _key_to_buf(key, &buf, &length)
        return list(self._ptr.get().query(buf, length, parallel))

    def save(self, str filename):
        self._ptr.get().save(filename.encode('utf-8'), 102)

    @staticmethod
    def load(str filename):
        cdef MultisetCSFUint64 obj = MultisetCSFUint64.__new__(MultisetCSFUint64)
        try:
            obj._ptr = cpp.MultisetCsf_uint64.load(filename.encode('utf-8'), 102)
        except RuntimeError as e:
            raise CsfDeserializationException(str(e))
        obj.permutation_seconds = 0.0
        obj.build_seconds = 0.0
        return obj

    @staticmethod
    def is_multiset():
        return True


cdef class MultisetCSFChar10:
    cdef shared_ptr[cpp.MultisetCsf_Char10] _ptr

    def __init__(self, list keys, values, prefilter=None, permutation=None,
                 bint shared_codebook=False, bint shared_filter=False, bint verbose=True):
        cdef vector[string] cpp_keys = _to_cpp_strings(keys)
        cdef vector[vector[cpp.Char10]] cpp_values

        values = np.asarray(values)
        if values.ndim != 2:
            raise ValueError("MultisetCSF values must be a 2D array.")

        cdef int num_cols = values.shape[1]
        cdef vector[cpp.Char10] col_vec
        cpp_values.reserve(num_cols)
        for col_idx in range(num_cols):
            col_vec.clear()
            for v in values[:, col_idx]:
                col_vec.push_back(_str_to_char10(v))
            cpp_values.push_back(col_vec)

        cdef cpp.MultisetConfig config
        if permutation is not None and isinstance(permutation, PermutationConfig):
            config.permutation_config = (<PermutationConfig>permutation)._ptr
        config.verbose = verbose
        config.shared_codebook = shared_codebook
        config.shared_filter = shared_filter
        if prefilter is not None and isinstance(prefilter, PreFilterConfig):
            config.filter_config = (<PreFilterConfig>prefilter)._ptr

        self._ptr = cpp.constructMultisetCsfConfig_Char10(cpp_keys, cpp_values, config)

    def query(self, key, bint parallel=False):
        cdef const char* buf
        cdef Py_ssize_t length
        _key_to_buf(key, &buf, &length)
        cdef vector[cpp.Char10] result = self._ptr.get().query(buf, length, parallel)
        py_results = []
        for i in range(result.size()):
            py_results.append(_char10_to_str(result[i]))
        return py_results

    def save(self, str filename):
        self._ptr.get().save(filename.encode('utf-8'), 103)

    @staticmethod
    def load(str filename):
        cdef MultisetCSFChar10 obj = MultisetCSFChar10.__new__(MultisetCSFChar10)
        try:
            obj._ptr = cpp.MultisetCsf_Char10.load(filename.encode('utf-8'), 103)
        except RuntimeError as e:
            raise CsfDeserializationException(str(e))
        return obj

    @staticmethod
    def is_multiset():
        return True


cdef class MultisetCSFChar12:
    cdef shared_ptr[cpp.MultisetCsf_Char12] _ptr

    def __init__(self, list keys, values, prefilter=None, permutation=None,
                 bint shared_codebook=False, bint shared_filter=False, bint verbose=True):
        cdef vector[string] cpp_keys = _to_cpp_strings(keys)
        cdef vector[vector[cpp.Char12]] cpp_values

        values = np.asarray(values)
        if values.ndim != 2:
            raise ValueError("MultisetCSF values must be a 2D array.")

        cdef int num_cols = values.shape[1]
        cdef vector[cpp.Char12] col_vec
        cpp_values.reserve(num_cols)
        for col_idx in range(num_cols):
            col_vec.clear()
            for v in values[:, col_idx]:
                col_vec.push_back(_str_to_char12(v))
            cpp_values.push_back(col_vec)

        cdef cpp.MultisetConfig config
        if permutation is not None and isinstance(permutation, PermutationConfig):
            config.permutation_config = (<PermutationConfig>permutation)._ptr
        config.verbose = verbose
        config.shared_codebook = shared_codebook
        config.shared_filter = shared_filter
        if prefilter is not None and isinstance(prefilter, PreFilterConfig):
            config.filter_config = (<PreFilterConfig>prefilter)._ptr

        self._ptr = cpp.constructMultisetCsfConfig_Char12(cpp_keys, cpp_values, config)

    def query(self, key, bint parallel=False):
        cdef const char* buf
        cdef Py_ssize_t length
        _key_to_buf(key, &buf, &length)
        cdef vector[cpp.Char12] result = self._ptr.get().query(buf, length, parallel)
        py_results = []
        for i in range(result.size()):
            py_results.append(_char12_to_str(result[i]))
        return py_results

    def save(self, str filename):
        self._ptr.get().save(filename.encode('utf-8'), 104)

    @staticmethod
    def load(str filename):
        cdef MultisetCSFChar12 obj = MultisetCSFChar12.__new__(MultisetCSFChar12)
        try:
            obj._ptr = cpp.MultisetCsf_Char12.load(filename.encode('utf-8'), 104)
        except RuntimeError as e:
            raise CsfDeserializationException(str(e))
        return obj

    @staticmethod
    def is_multiset():
        return True


cdef class MultisetCSFString:
    cdef shared_ptr[cpp.MultisetCsf_string] _ptr

    def __init__(self, list keys, values, prefilter=None, permutation=None,
                 bint shared_codebook=False, bint shared_filter=False, bint verbose=True):
        cdef vector[string] cpp_keys = _to_cpp_strings(keys)
        cdef vector[vector[string]] cpp_values

        if permutation is not None:
            raise ValueError("'permutation' is not supported for variable-length string values.")

        values = np.asarray(values)
        if values.ndim != 2:
            raise ValueError("MultisetCSF values must be a 2D array.")

        cdef int num_cols = values.shape[1]
        cdef vector[string] col_vec
        cpp_values.reserve(num_cols)
        for col_idx in range(num_cols):
            col_vec.clear()
            for v in values[:, col_idx]:
                if isinstance(v, str):
                    col_vec.push_back((<str>v).encode('utf-8'))
                else:
                    col_vec.push_back(<bytes>v)
            cpp_values.push_back(col_vec)

        cdef cpp.MultisetConfig config
        config.verbose = verbose
        config.shared_codebook = shared_codebook
        config.shared_filter = shared_filter
        if prefilter is not None and isinstance(prefilter, PreFilterConfig):
            config.filter_config = (<PreFilterConfig>prefilter)._ptr

        self._ptr = cpp.constructMultisetCsfConfig_string(cpp_keys, cpp_values, config)

    def query(self, key, bint parallel=False):
        cdef const char* buf
        cdef Py_ssize_t length
        _key_to_buf(key, &buf, &length)
        cdef vector[string] result = self._ptr.get().query(buf, length, parallel)
        return [result[i].decode('utf-8') for i in range(result.size())]

    def save(self, str filename):
        self._ptr.get().save(filename.encode('utf-8'), 105)

    @staticmethod
    def load(str filename):
        cdef MultisetCSFString obj = MultisetCSFString.__new__(MultisetCSFString)
        try:
            obj._ptr = cpp.MultisetCsf_string.load(filename.encode('utf-8'), 105)
        except RuntimeError as e:
            raise CsfDeserializationException(str(e))
        return obj

    @staticmethod
    def is_multiset():
        return True


# ── RaggedMultisetCSF wrappers ────────────────────────────────────────────

cdef class RaggedMultisetCSFUint32:
    cdef shared_ptr[cpp.RaggedMultisetCsf_uint32] _ptr

    def __init__(self, list keys, values, prefilter=None, permutation=None,
                 bint shared_codebook=False, bint verbose=True):
        cdef vector[string] cpp_keys = _to_cpp_strings(keys)
        cdef vector[vector[unsigned int]] cpp_values

        for row in values:
            cpp_values.push_back(<vector[unsigned int]>[<unsigned int>v for v in row])

        cdef cpp.MultisetConfig config
        if permutation is not None and isinstance(permutation, PermutationConfig):
            config.permutation_config = (<PermutationConfig>permutation)._ptr
        config.verbose = verbose
        config.shared_codebook = shared_codebook
        if prefilter is not None and isinstance(prefilter, PreFilterConfig):
            config.filter_config = (<PreFilterConfig>prefilter)._ptr

        self._ptr = cpp.constructRaggedMultisetCsf_uint32(cpp_keys, cpp_values, config)

    def query(self, key):
        cdef const char* buf
        cdef Py_ssize_t length
        _key_to_buf(key, &buf, &length)
        return list(self._ptr.get().query(buf, length))

    def save(self, str filename):
        self._ptr.get().save(filename.encode('utf-8'), 301)

    @staticmethod
    def load(str filename):
        cdef RaggedMultisetCSFUint32 obj = RaggedMultisetCSFUint32.__new__(RaggedMultisetCSFUint32)
        try:
            obj._ptr = cpp.RaggedMultisetCsf_uint32.load(filename.encode('utf-8'), 301)
        except RuntimeError as e:
            raise CsfDeserializationException(str(e))
        return obj

    @staticmethod
    def is_multiset():
        return True


# ── PackedCSF wrappers ───────────────────────────────────────────────────

cdef class PackedCSFUint32:
    cdef shared_ptr[cpp.PackedCsf_uint32] _ptr

    def __init__(self, list keys, values, bint verbose=True):
        cdef vector[string] cpp_keys = _to_cpp_strings(keys)
        cdef vector[vector[unsigned int]] cpp_values

        for row in values:
            cpp_values.push_back(<vector[unsigned int]>[<unsigned int>v for v in row])

        self._ptr = cpp.constructPackedCsf_uint32(cpp_keys, cpp_values, verbose)

    def query(self, key):
        cdef const char* buf
        cdef Py_ssize_t length
        _key_to_buf(key, &buf, &length)
        return list(self._ptr.get().query(buf, length))

    def save(self, str filename):
        self._ptr.get().save(filename.encode('utf-8'), 201)

    @staticmethod
    def load(str filename):
        cdef PackedCSFUint32 obj = PackedCSFUint32.__new__(PackedCSFUint32)
        try:
            obj._ptr = cpp.PackedCsf_uint32.load(filename.encode('utf-8'), 201)
        except RuntimeError as e:
            raise CsfDeserializationException(str(e))
        return obj

    @staticmethod
    def is_multiset():
        return True


# ── UnorderedMapBaseline ───────────────────────────────────────────────────

cdef class UnorderedMapBaseline:
    cdef cpp.CppUnorderedMapBaseline *_ptr

    def __init__(self, list keys, list values):
        cdef vector[string] cpp_keys = _to_cpp_strings(keys)
        cdef vector[unsigned int] cpp_values

        for v in values:
            cpp_values.push_back(<unsigned int>v)

        self._ptr = new cpp.CppUnorderedMapBaseline(cpp_keys, cpp_values)

    def __dealloc__(self):
        if self._ptr:
            del self._ptr

    def query(self, key):
        return self._ptr.query(_to_cpp_string(key))

    def size(self):
        return self._ptr.size()


# ── Permutation functions ─────────────────────────────────────────────────

def permute_uint32(np.ndarray[np.uint32_t, ndim=2] array not None, config=None):
    cdef int num_rows = array.shape[0]
    cdef int num_cols = array.shape[1]
    if isinstance(config, GlobalSortPermutationConfig):
        cpp.globalSortPermutation_uint32(<unsigned int*>array.data, num_rows, num_cols,
                                         (<GlobalSortPermutationConfig>config)._refinement_iterations)
    else:
        cpp.entropyPermutation_uint32(<unsigned int*>array.data, num_rows, num_cols)

def permute_uint64(np.ndarray[np.uint64_t, ndim=2] array not None, config=None):
    cdef int num_rows = array.shape[0]
    cdef int num_cols = array.shape[1]
    if isinstance(config, GlobalSortPermutationConfig):
        cpp.globalSortPermutation_uint64(<unsigned long long*>array.data, num_rows, num_cols,
                                         (<GlobalSortPermutationConfig>config)._refinement_iterations)
    else:
        cpp.entropyPermutation_uint64(<unsigned long long*>array.data, num_rows, num_cols)

def permute_char10(np.ndarray array not None, config=None):
    if array.ndim != 2:
        raise RuntimeError("Input should be a 2D numpy array.")
    cdef int num_rows = array.shape[0]
    cdef int num_cols = array.shape[1]
    if isinstance(config, GlobalSortPermutationConfig):
        cpp.globalSortPermutation_Char10(<cpp.Char10*>array.data, num_rows, num_cols,
                                         (<GlobalSortPermutationConfig>config)._refinement_iterations)
    else:
        cpp.entropyPermutation_Char10(<cpp.Char10*>array.data, num_rows, num_cols)

def permute_char12(np.ndarray array not None, config=None):
    if array.ndim != 2:
        raise RuntimeError("Input should be a 2D numpy array.")
    cdef int num_rows = array.shape[0]
    cdef int num_cols = array.shape[1]
    if isinstance(config, GlobalSortPermutationConfig):
        cpp.globalSortPermutation_Char12(<cpp.Char12*>array.data, num_rows, num_cols,
                                         (<GlobalSortPermutationConfig>config)._refinement_iterations)
    else:
        cpp.entropyPermutation_Char12(<cpp.Char12*>array.data, num_rows, num_cols)
