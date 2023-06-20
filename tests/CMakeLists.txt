enable_testing()

include(GoogleTest)

add_executable(DummyTest DummyTest.cc)
target_link_libraries(DummyTest PRIVATE gtest_main caramel)
gtest_discover_tests(DummyTest)

add_executable(BitArrayTest BitArrayTest.cc)
target_link_libraries(BitArrayTest PRIVATE gtest_main caramel)
gtest_discover_tests(BitArrayTest)