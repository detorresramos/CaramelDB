# distutils: language = c++
# distutils: include_dirs = src

from libcpp.string cimport string
from libcpp.memory cimport shared_ptr
from libcpp cimport bool
from libcpp.vector cimport vector
from libcpp.utility cimport pair
from libc.stdint cimport uint32_t


cdef extern from "src/construct/BloomFilter.h" namespace "caramel":
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
        self.cpp_bloom_filter.get().add(key)

    def contains(self, key):
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
        shared_ptr[Csf[T]] load(const string &filename, const uint32_t type_id)


cdef extern from "src/construct/Construct.h" namespace "caramel":
    shared_ptr[Csf[T]] constructCsf[T](const vector[string] &keys,
                                       const vector[T] &values,
                                       bool use_bloom_filter,
                                       bool verbose) except +


cdef class PyCsfInt:
    cdef shared_ptr[Csf[int]] cpp_csf
    
    @staticmethod
    def construct(vector[string] keys, vector[int] values, bint use_bloom_filter=True, bint verbose=True):
        cdef PyCsfInt obj = PyCsfInt()
        obj.cpp_csf = constructCsf(keys, values, use_bloom_filter, verbose)
        return obj

    def query(self, key):
        return self.cpp_csf.get().query(key)

    def save(self, filename):
        self.cpp_csf.get().save(filename, 0)

    @staticmethod
    def load(filename):
        cdef PyCsfInt obj = PyCsfInt.__new__(PyCsfInt)
        obj.cpp_csf = Csf[int].load(filename, 0)
        return obj
