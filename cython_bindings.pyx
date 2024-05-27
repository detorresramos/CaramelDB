# distutils: language = c++

from cython_bindings cimport BloomFilter


cdef class PyBloomFilter:
    cdef BloomFilter* c_bloom

    def __cinit__(self, size_t num_elements, float error_rate):
        self.c_bloom = new BloomFilter(num_elements, error_rate)

    def size(self):
        return self.c_bloom.size()
    
    def num_hashes(self):
        return self.c_bloom.numHashes()

    def contains(self, string key):
        return self.c_bloom.contains(key)
