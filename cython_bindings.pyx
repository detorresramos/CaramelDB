# distutils: language = c++
# distutils: include_dirs = src

from libcpp.string cimport string
from libcpp.memory cimport shared_ptr
from libcpp cimport bool


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
