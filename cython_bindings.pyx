from cython_bindings cimport Csf
from cython_bindings cimport MultisetCsf
from cython_bindings cimport constructCsf
from cython_bindings cimport constructMultisetCsf
from cython_bindings cimport entropyPermutation
from cython_bindings cimport T
from libcpp.string cimport string
from libcpp.vector cimport vector


cdef class PyCsf:
    cdef Csf* c_obj

    def __cinit__(self, list keys, list values, bool use_bloom_filter=True, bool verbose=True):
        cdef vector[string] cpp_keys
        for key in keys:
            cpp_keys.push_back(key)
        cdef vector cpp_values
        for val in values:
            cpp_values.push_back(val)
        cdef bool cpp_use_bloom_filter = use_bloom_filter
        cdef bool cpp_verbose = verbose
        self.c_obj = new Csf(constructCsf(cpp_keys, cpp_values, cpp_use_bloom_filter, cpp_verbose))

    def __dealloc__(self):
        del self.c_obj

    def query(self, string key):
        return self.c_obj.query(key)

    def save(self, string filename):
        return self.c_obj.save(filename)

    @staticmethod
    def load(cls, string filename):
        return Csf.load(filename)

    def is_multiset(self):
        return False

    def bloom_filter(self):
        return self.c_obj.bloomFilter()

cdef class PyMultisetCsf:
    cdef MultisetCsf* c_obj

    def __cinit__(self, list keys, list values, bool use_bloom_filter=True, bool verbose=True):
        cdef vector[string] cpp_keys
        for key in keys:
            cpp_keys.push_back(key)
        cdef vector[vector] cpp_values
        for val_list in values:
            cpp_val_vector = vector()
            for val in val_list:
                cpp_val_vector.push_back(val)
            cpp_values.push_back(cpp_val_vector)
        cdef bool cpp_use_bloom_filter = use_bloom_filter
        cdef bool cpp_verbose = verbose
        self.c_obj = new MultisetCsf(constructMultisetCsf(cpp_keys, cpp_values, cpp_use_bloom_filter, cpp_verbose))

    def __dealloc__(self):
        del self.c_obj

    def query(self, string key, bool parallel=True):
        return self.c_obj.query(key, parallel)

    def save(self, string filename):
        return self.c_obj.save(filename)

    @staticmethod
    def load(cls, string filename):
        return MultisetCsf.load(filename)

    def is_multiset(self):
        return True

def permute(array):
    cdef:
        int num_rows, num_cols
        T* M

    if array.ndim != 2:
        raise RuntimeError("Input should be a 2D numpy array.")
    
    num_rows, num_cols = array.shape
    M = <T*> array.data
    entropyPermutation(M, num_rows, num_cols)
