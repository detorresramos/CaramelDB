enable_testing()

include(GoogleTest)

set(CARAMEL_TESTS
    BitArrayTest
    BucketedHashStoreTest
    CodecTest
    ConstructCsfTest
    EntropyPermutationTest
    GaussianEliminationTest
    HypergraphPeelerTest
    LazyGaussianEliminationTest
    SolveTest
    BloomFilterTest
    XorFilterTest
    BinaryFuseFilterTest
    FilterSizeComparisonTest)

foreach(CARAMEL_TEST IN LISTS CARAMEL_TESTS)
    add_executable(${CARAMEL_TEST} ${CARAMEL_TEST}.cc)
    target_link_libraries(${CARAMEL_TEST} PRIVATE gtest_main caramel_lib)
    gtest_discover_tests(${CARAMEL_TEST})
endforeach()