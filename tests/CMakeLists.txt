enable_testing()

include(GoogleTest)

set(CARAMEL_TESTS
    AutoFilterSelectionTest
    ConstructMultisetTest
    BitArrayTest
    StagedBuildTest
    RibbonTest
    CodecTest
    ConstructCsfTest
    EntropyPermutationTest
    GlobalSortPermutationTest
    BloomFilterTest
    XorFilterTest
    BitPackedXorFilterTest
    BinaryFuseFilterTest
    BitPackedArrayTest
    FilterSizeComparisonTest)

foreach(CARAMEL_TEST IN LISTS CARAMEL_TESTS)
    add_executable(${CARAMEL_TEST} ${CARAMEL_TEST}.cc)
    target_link_libraries(${CARAMEL_TEST} PRIVATE gtest_main caramel_lib)
    gtest_discover_tests(${CARAMEL_TEST})
endforeach()