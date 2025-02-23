# distutils: language = c++
# distutils: include_dirs = src

from libc.stdint cimport uint32_t
from libcpp cimport bool
from libcpp.memory cimport shared_ptr
from libcpp.string cimport string
from libcpp.utility cimport pair
from libcpp.vector cimport vector


# cdef extern from "src/construct/filter/FilterConfig.h" namespace "caramel":
#     cdef cppclass PreFilterConfig:
#         pass
#     ctypedef shared_ptr[PreFilterConfig] PreFilterConfigPtr

#     cdef cppclass BloomPreFilterConfig(PreFilterConfig):
#         BloomPreFilterConfig() except +

# cdef class PyPreFilterConfig:
#     cdef PreFilterConfigPtr cpp_prefilter

#     def __cinit__(self):
#         self.cpp_prefilter = PreFilterConfigPtr()  # null pointer by default


# cdef class PyBloomPreFilterConfig(PyPreFilterConfig):
#     def __cinit__(self):
#         self.cpp_prefilter = shared_ptr[PreFilterConfig](new BloomPreFilterConfig())


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


# cdef extern from "src/construct/Csf.h" namespace "caramel":
#     cdef cppclass CsfDeserializationException "CsfDeserializationException":
#         CsfDeserializationException(const string& message)

#     cdef cppclass Csf[T]:
#         T query(const string &key) const
#         void save(const string &filename, const uint32_t type_id) const
#         @staticmethod
#         shared_ptr[Csf[T]] load(const string &filename, const uint32_t type_id)


# cdef extern from "src/construct/Construct.h" namespace "caramel":
#     shared_ptr[Csf[T]] constructCsf[T](const vector[string] &keys,
#                                        const vector[T] &values,
#                                        PreFilterConfigPtr prefilter,
#                                        bool verbose) except +


# cdef extern from "src/construct/MultisetCsf.h" namespace "caramel":
#     cdef cppclass MultisetCsf[T]:
#         vector[T] query(const string &key, bool parallelize) const
#         void save(const string &filename, const uint32_t type_id) const
#         @staticmethod
#         shared_ptr[MultisetCsf[T]] load(const string &filename, const uint32_t type_id)


# cdef extern from "src/construct/ConstructMultiset.h" namespace "caramel":
#     shared_ptr[MultisetCsf[T]] constructMultisetCsf[T](const vector[string] &keys,
#                                        const vector[vector[T]] &values,
#                                        PreFilterConfigPtr prefilter,
#                                        bool verbose) except +


# # cdef extern from "<array>" namespace "std" nogil:
# #     cdef cppclass char10array "std::array<char, 10>":
# #         char10array() except +
# #         char *data() nogil


# cdef class PyCsfUint32:
#     cdef shared_ptr[Csf[uint32_t]] cpp_csf
    
#     # we have a static constructor because I couldn't get the load method to 
#     # work properly if this logic was defined in the __cinit__ method
#     # if we can get that to work we can move this logic to the __cinit__ method
#     # and remove the checks for self.cpp_csf
#     @staticmethod
#     def construct(vector[string] keys, vector[uint32_t] values, prefilter=None, bint verbose=True):
#         cdef PreFilterConfigPtr prefilter_ptr
#         if prefilter is None:
#             prefilter_ptr = PreFilterConfigPtr()
#         elif isinstance(prefilter, PyPreFilterConfig):
#             prefilter_ptr = <PreFilterConfigPtr> prefilter.cpp_prefilter
#         else:
#             raise TypeError("prefilter must be a PyPreFilterConfig or None")
#         cdef PyCsfUint32 obj = PyCsfUint32()
#         obj.cpp_csf = constructCsf(keys, values, prefilter_ptr, verbose)
#         return obj

#     def query(self, key):
#         if not self.cpp_csf:
#             raise ValueError("CSF object not built properly, please use the static construct method instead.")
#         return self.cpp_csf.get().query(key)

#     def save(self, filename):
#         if not self.cpp_csf:
#             raise ValueError("CSF object not built properly, please use the static construct method instead.")
#         self.cpp_csf.get().save(filename, 0)

#     @staticmethod
#     def load(filename):
#         cdef PyCsfUint32 obj = PyCsfUint32.__new__(PyCsfUint32)
#         obj.cpp_csf = Csf[uint32_t].load(filename, 0)
#         return obj


# cdef class PyMultisetCsfUint32:
#     cdef shared_ptr[MultisetCsf[uint32_t]] cpp_csf
    
#     @staticmethod
#     def construct(vector[string] keys, vector[vector[uint32_t]] values, prefilter=None, bint verbose=True):
#         cdef PreFilterConfigPtr prefilter_ptr
#         if prefilter is None:
#             prefilter_ptr = PreFilterConfigPtr()
#         elif isinstance(prefilter, PyPreFilterConfig):
#             prefilter_ptr = (<PyPreFilterConfig>prefilter).get_config()
#         else:
#             raise TypeError("prefilter must be a PyPreFilterConfig or None")
#         cdef PyMultisetCsfUint32 obj = PyMultisetCsfUint32()
#         obj.cpp_csf = constructMultisetCsf(keys, values, prefilter_ptr, verbose)
#         return obj

#     def query(self, key, parallelize):
#         if not self.cpp_csf:
#             raise ValueError("CSF object not built properly, please use the static construct method instead.")
#         return self.cpp_csf.get().query(key, parallelize)

#     def save(self, filename):
#         if not self.cpp_csf:
#             raise ValueError("CSF object not built properly, please use the static construct method instead.")
#         self.cpp_csf.get().save(filename, 0)

#     @staticmethod
#     def load(filename):
#         cdef PyMultisetCsfUint32 obj = PyMultisetCsfUint32.__new__(PyMultisetCsfUint32)
#         obj.cpp_csf = MultisetCsf[uint32_t].load(filename, 0)
#         return obj
