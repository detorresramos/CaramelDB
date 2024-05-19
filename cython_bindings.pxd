from libcpp cimport bool
from libcpp.string cimport string
from libcpp.vector cimport vector


cdef extern from "src/construct/BloomFilter.h" namespace "caramel":
    cdef cppclass BloomFilter:
        size_t size()
        size_t numHashes()
        bool contains(string)

cdef extern from "src/construct/Csf.h" namespace "caramel":
    cdef cppclass Csf[T]:
        T query(string)
        bool save(string, uint32_t)
        @staticmethod
        Csf[T] load(string, uint32_t)
        bool is_multiset()
        BloomFilter bloomFilter()

cdef extern from "src/construct/Construct.h" namespace "caramel":
    Csf[T] constructCsf(vector[string], vector[T], bool, bool)

cdef extern from "src/construct/MultisetCsf.h" namespace "caramel":
    cdef cppclass MultisetCsf[T]:
        vector[T] query(string, bool)
        bool save(string, uint32_t)
        @staticmethod
        MultisetCsf[T] load(string, uint32_t)
        bool is_multiset()

cdef extern from "src/construct/ConstructMultiset.h" namespace "caramel":
    MultisetCsf[T] constructMultisetCsf(vector[string], vector[vector[T]], bool, bool)

cdef extern from "src/construct/EntropyPermutation.h" namespace "caramel":
    void entropyPermutation[T](T*, int, int)