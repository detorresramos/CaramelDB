# distutils: language = c++

from libcpp.string cimport string
from libcpp.vector cimport vector
from libcpp.memory cimport shared_ptr
from libcpp cimport bool as cbool
from libc.stddef cimport size_t

# ── Char10/Char12 typedefs ────────────────────────────────────────────────

cdef extern from "cython/caramel_types.h":
    cppclass Char10:
        char& operator[](size_t)
        char* data()
    cppclass Char12:
        char& operator[](size_t)
        char* data()


# ── std::optional ─────────────────────────────────────────────────────────

cdef extern from "<optional>" namespace "std" nogil:
    cppclass optional[T]:
        optional()
        optional(T&)
        cbool has_value()
        T& value()

# ── Stats ──────────────────────────────────────────────────────────────────

cdef extern from "src/construct/CsfStats.h" namespace "caramel":
    cppclass FilterStats:
        string type
        size_t size_bytes
        size_t num_elements
        optional[size_t] num_hashes
        optional[size_t] size_bits
        optional[int] fingerprint_bits

    cppclass HuffmanStats:
        size_t num_unique_symbols
        unsigned int max_code_length
        double avg_bits_per_symbol
        vector[unsigned int] code_length_distribution

    cppclass BucketStats:
        size_t num_buckets
        size_t total_solution_bits
        double avg_solution_bits
        size_t min_solution_bits
        size_t max_solution_bits

    cppclass CsfStats:
        size_t in_memory_bytes
        BucketStats bucket_stats
        HuffmanStats huffman_stats
        optional[FilterStats] filter_stats
        double solution_bytes
        double filter_bytes
        double metadata_bytes

# ── Filter Config ──────────────────────────────────────────────────────────

cdef extern from "src/construct/filter/FilterConfig.h" namespace "caramel":
    cppclass PreFilterConfig:
        pass

    cppclass BloomPreFilterConfig(PreFilterConfig):
        BloomPreFilterConfig(size_t bits_per_element, size_t num_hashes) except +
        size_t bits_per_element
        size_t num_hashes

    cppclass XORPreFilterConfig(PreFilterConfig):
        XORPreFilterConfig(int fingerprint_bits) except +
        int fingerprint_bits

    cppclass BinaryFusePreFilterConfig(PreFilterConfig):
        BinaryFusePreFilterConfig(int fingerprint_bits) except +
        int fingerprint_bits

    cppclass AutoPreFilterConfig(PreFilterConfig):
        AutoPreFilterConfig() except +

# ── PreFilter ──────────────────────────────────────────────────────────────

cdef extern from "src/construct/filter/PreFilter.h" namespace "caramel":
    cppclass PreFilter_uint32 "caramel::PreFilter<uint32_t>":
        cbool contains(const string &key)
        optional[unsigned int] getMostCommonValue()
        optional[FilterStats] getStats()

    cppclass PreFilter_uint64 "caramel::PreFilter<uint64_t>":
        cbool contains(const string &key)
        optional[unsigned long long] getMostCommonValue()
        optional[FilterStats] getStats()

    cppclass PreFilter_Char10 "caramel::PreFilter<Char10>":
        cbool contains(const string &key)
        optional[Char10] getMostCommonValue()
        optional[FilterStats] getStats()

    cppclass PreFilter_Char12 "caramel::PreFilter<Char12>":
        cbool contains(const string &key)
        optional[Char12] getMostCommonValue()
        optional[FilterStats] getStats()

    cppclass PreFilter_string "caramel::PreFilter<std::string>":
        cbool contains(const string &key)
        optional[string] getMostCommonValue()
        optional[FilterStats] getStats()

# ── PreFilter save helpers ─────────────────────────────────────────────────

cdef extern from "cython/caramel_types.h":
    void savePreFilter_uint32 "savePreFilter<uint32_t>"(shared_ptr[PreFilter_uint32] filter, const string &filename) except +
    void savePreFilter_uint64 "savePreFilter<uint64_t>"(shared_ptr[PreFilter_uint64] filter, const string &filename) except +
    void savePreFilter_Char10 "savePreFilter<Char10>"(shared_ptr[PreFilter_Char10] filter, const string &filename) except +
    void savePreFilter_Char12 "savePreFilter<Char12>"(shared_ptr[PreFilter_Char12] filter, const string &filename) except +
    void savePreFilter_string "savePreFilter<std::string>"(shared_ptr[PreFilter_string] filter, const string &filename) except +

# ── PreFilter subclasses ───────────────────────────────────────────────────

cdef extern from "src/construct/filter/BloomPreFilter.h" namespace "caramel":
    cppclass BloomPreFilter_uint32 "caramel::BloomPreFilter<uint32_t>"(PreFilter_uint32):
        void save(const string &filename) except +
        @staticmethod
        shared_ptr[BloomPreFilter_uint32] load(const string &filename) except +

    cppclass BloomPreFilter_uint64 "caramel::BloomPreFilter<uint64_t>"(PreFilter_uint64):
        void save(const string &filename) except +
        @staticmethod
        shared_ptr[BloomPreFilter_uint64] load(const string &filename) except +

    cppclass BloomPreFilter_Char10 "caramel::BloomPreFilter<Char10>"(PreFilter_Char10):
        void save(const string &filename) except +
        @staticmethod
        shared_ptr[BloomPreFilter_Char10] load(const string &filename) except +

    cppclass BloomPreFilter_Char12 "caramel::BloomPreFilter<Char12>"(PreFilter_Char12):
        void save(const string &filename) except +
        @staticmethod
        shared_ptr[BloomPreFilter_Char12] load(const string &filename) except +

    cppclass BloomPreFilter_string "caramel::BloomPreFilter<std::string>"(PreFilter_string):
        void save(const string &filename) except +
        @staticmethod
        shared_ptr[BloomPreFilter_string] load(const string &filename) except +

cdef extern from "src/construct/filter/XORPreFilter.h" namespace "caramel":
    cppclass XORPreFilter_uint32 "caramel::XORPreFilter<uint32_t>"(PreFilter_uint32):
        void save(const string &filename) except +
        @staticmethod
        shared_ptr[XORPreFilter_uint32] load(const string &filename) except +

    cppclass XORPreFilter_uint64 "caramel::XORPreFilter<uint64_t>"(PreFilter_uint64):
        void save(const string &filename) except +
        @staticmethod
        shared_ptr[XORPreFilter_uint64] load(const string &filename) except +

    cppclass XORPreFilter_Char10 "caramel::XORPreFilter<Char10>"(PreFilter_Char10):
        void save(const string &filename) except +
        @staticmethod
        shared_ptr[XORPreFilter_Char10] load(const string &filename) except +

    cppclass XORPreFilter_Char12 "caramel::XORPreFilter<Char12>"(PreFilter_Char12):
        void save(const string &filename) except +
        @staticmethod
        shared_ptr[XORPreFilter_Char12] load(const string &filename) except +

    cppclass XORPreFilter_string "caramel::XORPreFilter<std::string>"(PreFilter_string):
        void save(const string &filename) except +
        @staticmethod
        shared_ptr[XORPreFilter_string] load(const string &filename) except +

cdef extern from "src/construct/filter/BinaryFusePreFilter.h" namespace "caramel":
    cppclass BinaryFusePreFilter_uint32 "caramel::BinaryFusePreFilter<uint32_t>"(PreFilter_uint32):
        void save(const string &filename) except +
        @staticmethod
        shared_ptr[BinaryFusePreFilter_uint32] load(const string &filename) except +

    cppclass BinaryFusePreFilter_uint64 "caramel::BinaryFusePreFilter<uint64_t>"(PreFilter_uint64):
        void save(const string &filename) except +
        @staticmethod
        shared_ptr[BinaryFusePreFilter_uint64] load(const string &filename) except +

    cppclass BinaryFusePreFilter_Char10 "caramel::BinaryFusePreFilter<Char10>"(PreFilter_Char10):
        void save(const string &filename) except +
        @staticmethod
        shared_ptr[BinaryFusePreFilter_Char10] load(const string &filename) except +

    cppclass BinaryFusePreFilter_Char12 "caramel::BinaryFusePreFilter<Char12>"(PreFilter_Char12):
        void save(const string &filename) except +
        @staticmethod
        shared_ptr[BinaryFusePreFilter_Char12] load(const string &filename) except +

    cppclass BinaryFusePreFilter_string "caramel::BinaryFusePreFilter<std::string>"(PreFilter_string):
        void save(const string &filename) except +
        @staticmethod
        shared_ptr[BinaryFusePreFilter_string] load(const string &filename) except +

# ── BloomFilter (standalone) ───────────────────────────────────────────────

cdef extern from "src/construct/filter/BloomFilter.h" namespace "caramel":
    cppclass BloomFilter:
        void add(const string &key)
        cbool contains(const string &key)
        size_t size()
        size_t numHashes()

        @staticmethod
        shared_ptr[BloomFilter] makeAutotuned(size_t num_elements, double error_rate, cbool verbose)

        @staticmethod
        shared_ptr[BloomFilter] makeFixed(size_t bitarray_size, size_t num_hashes)

# ── CsfDeserializationException ────────────────────────────────────────────

cdef extern from "src/construct/Csf.h" namespace "caramel":
    cppclass CsfDeserializationException:
        CsfDeserializationException(const string &message)

# ── Csf ────────────────────────────────────────────────────────────────────

cdef extern from "src/construct/Csf.h" namespace "caramel":
    cppclass Csf_uint32 "caramel::Csf<uint32_t>":
        unsigned int query(const char *data, size_t length) except +
        unsigned int query(const string &key) except +
        shared_ptr[PreFilter_uint32] getFilter()
        CsfStats getStats()
        void save(const string &filename, unsigned int type_id) except +

        @staticmethod
        shared_ptr[Csf_uint32] load(const string &filename, unsigned int type_id) except +

    cppclass Csf_uint64 "caramel::Csf<uint64_t>":
        unsigned long long query(const char *data, size_t length) except +
        unsigned long long query(const string &key) except +
        shared_ptr[PreFilter_uint64] getFilter()
        CsfStats getStats()
        void save(const string &filename, unsigned int type_id) except +

        @staticmethod
        shared_ptr[Csf_uint64] load(const string &filename, unsigned int type_id) except +

    cppclass Csf_Char10 "caramel::Csf<Char10>":
        Char10 query(const char *data, size_t length) except +
        Char10 query(const string &key) except +
        shared_ptr[PreFilter_Char10] getFilter()
        CsfStats getStats()
        void save(const string &filename, unsigned int type_id) except +

        @staticmethod
        shared_ptr[Csf_Char10] load(const string &filename, unsigned int type_id) except +

    cppclass Csf_Char12 "caramel::Csf<Char12>":
        Char12 query(const char *data, size_t length) except +
        Char12 query(const string &key) except +
        shared_ptr[PreFilter_Char12] getFilter()
        CsfStats getStats()
        void save(const string &filename, unsigned int type_id) except +

        @staticmethod
        shared_ptr[Csf_Char12] load(const string &filename, unsigned int type_id) except +

    cppclass Csf_string "caramel::Csf<std::string>":
        string query(const char *data, size_t length) except +
        string query(const string &key) except +
        shared_ptr[PreFilter_string] getFilter()
        CsfStats getStats()
        void save(const string &filename, unsigned int type_id) except +

        @staticmethod
        shared_ptr[Csf_string] load(const string &filename, unsigned int type_id) except +

# ── Construct free functions ───────────────────────────────────────────────

cdef extern from "src/construct/Construct.h" namespace "caramel":
    shared_ptr[Csf_uint32] constructCsf_uint32 "caramel::constructCsf<uint32_t>"(
        const vector[string] &keys, const vector[unsigned int] &values,
        shared_ptr[PreFilterConfig] filter_config, cbool verbose) except +

    shared_ptr[Csf_uint64] constructCsf_uint64 "caramel::constructCsf<uint64_t>"(
        const vector[string] &keys, const vector[unsigned long long] &values,
        shared_ptr[PreFilterConfig] filter_config, cbool verbose) except +

    shared_ptr[Csf_Char10] constructCsf_Char10 "caramel::constructCsf<Char10>"(
        const vector[string] &keys, const vector[Char10] &values,
        shared_ptr[PreFilterConfig] filter_config, cbool verbose) except +

    shared_ptr[Csf_Char12] constructCsf_Char12 "caramel::constructCsf<Char12>"(
        const vector[string] &keys, const vector[Char12] &values,
        shared_ptr[PreFilterConfig] filter_config, cbool verbose) except +

    shared_ptr[Csf_string] constructCsf_string "caramel::constructCsf<std::string>"(
        const vector[string] &keys, const vector[string] &values,
        shared_ptr[PreFilterConfig] filter_config, cbool verbose) except +

# ── MultisetCsf ───────────────────────────────────────────────────────────

cdef extern from "src/construct/multiset/MultisetCsf.h" namespace "caramel":
    cppclass MultisetCsf_uint32 "caramel::MultisetCsf<uint32_t>":
        vector[unsigned int] query(const char *data, size_t length, cbool parallelize) except +
        vector[unsigned int] query(const string &key, cbool parallelize) except +
        void save(const string &filename, unsigned int type_id) except +

        @staticmethod
        shared_ptr[MultisetCsf_uint32] load(const string &filename, unsigned int type_id) except +

    cppclass MultisetCsf_uint64 "caramel::MultisetCsf<uint64_t>":
        vector[unsigned long long] query(const char *data, size_t length, cbool parallelize) except +
        vector[unsigned long long] query(const string &key, cbool parallelize) except +
        void save(const string &filename, unsigned int type_id) except +

        @staticmethod
        shared_ptr[MultisetCsf_uint64] load(const string &filename, unsigned int type_id) except +

    cppclass MultisetCsf_Char10 "caramel::MultisetCsf<Char10>":
        vector[Char10] query(const char *data, size_t length, cbool parallelize) except +
        vector[Char10] query(const string &key, cbool parallelize) except +
        void save(const string &filename, unsigned int type_id) except +

        @staticmethod
        shared_ptr[MultisetCsf_Char10] load(const string &filename, unsigned int type_id) except +

    cppclass MultisetCsf_Char12 "caramel::MultisetCsf<Char12>":
        vector[Char12] query(const char *data, size_t length, cbool parallelize) except +
        vector[Char12] query(const string &key, cbool parallelize) except +
        void save(const string &filename, unsigned int type_id) except +

        @staticmethod
        shared_ptr[MultisetCsf_Char12] load(const string &filename, unsigned int type_id) except +

    cppclass MultisetCsf_string "caramel::MultisetCsf<std::string>":
        vector[string] query(const char *data, size_t length, cbool parallelize) except +
        vector[string] query(const string &key, cbool parallelize) except +
        void save(const string &filename, unsigned int type_id) except +

        @staticmethod
        shared_ptr[MultisetCsf_string] load(const string &filename, unsigned int type_id) except +

# ── MultisetConfig ────────────────────────────────────────────────────────

cdef extern from "src/construct/multiset/permute/PermutationConfig.h" namespace "caramel":
    cppclass PermutationConfig:
        pass

    cppclass EntropyPermutationConfig(PermutationConfig):
        EntropyPermutationConfig()

    cppclass GlobalSortPermutationConfig(PermutationConfig):
        GlobalSortPermutationConfig() except +
        GlobalSortPermutationConfig(int refinement_iterations) except +
        int refinement_iterations

cdef extern from "src/construct/multiset/MultisetConfig.h" namespace "caramel":
    cppclass MultisetConfig:
        MultisetConfig()
        shared_ptr[PermutationConfig] permutation_config
        shared_ptr[PreFilterConfig] filter_config
        cbool shared_codebook
        cbool shared_filter
        cbool verbose

# ── ConstructMultiset free functions ───────────────────────────────────────

cdef extern from "src/construct/multiset/ConstructMultiset.h" namespace "caramel":
    shared_ptr[MultisetCsf_uint32] constructMultisetCsfConfig_uint32 "caramel::constructMultisetCsf<uint32_t>"(
        const vector[string] &keys, const vector[vector[unsigned int]] &values,
        const MultisetConfig &config) except +

    shared_ptr[MultisetCsf_uint64] constructMultisetCsfConfig_uint64 "caramel::constructMultisetCsf<uint64_t>"(
        const vector[string] &keys, const vector[vector[unsigned long long]] &values,
        const MultisetConfig &config) except +

    shared_ptr[MultisetCsf_Char10] constructMultisetCsfConfig_Char10 "caramel::constructMultisetCsf<Char10>"(
        const vector[string] &keys, const vector[vector[Char10]] &values,
        const MultisetConfig &config) except +

    shared_ptr[MultisetCsf_Char12] constructMultisetCsfConfig_Char12 "caramel::constructMultisetCsf<Char12>"(
        const vector[string] &keys, const vector[vector[Char12]] &values,
        const MultisetConfig &config) except +

    shared_ptr[MultisetCsf_string] constructMultisetCsfConfig_string "caramel::constructMultisetCsf<std::string>"(
        const vector[string] &keys, const vector[vector[string]] &values,
        const MultisetConfig &config) except +

# ── UnorderedMapBaseline ───────────────────────────────────────────────────

cdef extern from "src/baselines/UnorderedMapBaseline.h" namespace "caramel":
    cppclass CppUnorderedMapBaseline "caramel::UnorderedMapBaseline":
        CppUnorderedMapBaseline(const vector[string] &keys, const vector[unsigned int] &values) except +
        unsigned int query(const string &key) except +
        size_t size()

# ── Permutation ────────────────────────────────────────────────────────────

cdef extern from "src/construct/multiset/permute/EntropyPermutation.h" namespace "caramel":
    void entropyPermutation_uint32 "caramel::entropyPermutation<uint32_t>"(unsigned int *M, int num_rows, int num_cols) nogil
    void entropyPermutation_uint64 "caramel::entropyPermutation<uint64_t>"(unsigned long long *M, int num_rows, int num_cols) nogil
    void entropyPermutation_Char10 "caramel::entropyPermutation<Char10>"(Char10 *M, int num_rows, int num_cols) nogil
    void entropyPermutation_Char12 "caramel::entropyPermutation<Char12>"(Char12 *M, int num_rows, int num_cols) nogil

cdef extern from "src/construct/multiset/permute/GlobalSortPermutation.h" namespace "caramel":
    void globalSortPermutation_uint32 "caramel::globalSortPermutation<uint32_t>"(unsigned int *M, int num_rows, int num_cols, int refinement_iterations) nogil
    void globalSortPermutation_uint64 "caramel::globalSortPermutation<uint64_t>"(unsigned long long *M, int num_rows, int num_cols, int refinement_iterations) nogil
    void globalSortPermutation_Char10 "caramel::globalSortPermutation<Char10>"(Char10 *M, int num_rows, int num_cols, int refinement_iterations) nogil
    void globalSortPermutation_Char12 "caramel::globalSortPermutation<Char12>"(Char12 *M, int num_rows, int num_cols, int refinement_iterations) nogil
