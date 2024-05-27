from libcpp cimport bool
from libcpp.string cimport string
from libcpp.vector cimport vector

cdef extern from "deps/cereal/include/cereal/access.hpp":
    pass

cdef extern from "deps/cereal/include/cereal/macros.hpp":
    pass

cdef extern from "src/construct/BloomFilter.h" namespace "caramel":
    cdef cppclass BloomFilter:
        BloomFilter(int, float)
        size_t size()
        size_t numHashes()
        bool contains(string)
