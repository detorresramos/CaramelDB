enable_testing()

include(GoogleTest)

add_executable(CSFTest CSFTest.cc)

target_link_libraries(CSFTest gtest gtest_main caramel)