enable_testing()

include(GoogleTest)

set(CARAMEL_TESTS
    BitArrayTest
    CodecTest
    BucketedHashStoreTest
    ConstructCsfTest
    GaussianEliminationTest
    HypergraphPeelerTest
    LazyGaussianEliminationTest
    SolveTest)

foreach(CARAMEL_TEST IN LISTS CARAMEL_TESTS)
    add_executable(${CARAMEL_TEST} ${CARAMEL_TEST}.cc)
    target_link_libraries(${CARAMEL_TEST} PRIVATE gtest_main caramel_lib)
    gtest_discover_tests(${CARAMEL_TEST})
endforeach()