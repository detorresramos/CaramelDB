cmake_minimum_required(VERSION 3.4...3.18)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

project(Caramel LANGUAGES CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# See [1] https://gcc.gnu.org/wiki/Visibility [2]
# https://stackoverflow.com/a/31157258/4565794
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

if(DEFINED ENV{CARAMEL_BUILD_MODE})
    set(CMAKE_BUILD_TYPE $ENV{CARAMEL_BUILD_MODE})
    message(STATUS "Setting build type to ${CMAKE_BUILD_TYPE} based on CARAMEL_BUILD_MODE environment variable.")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG)
endif()

# Project versioning
find_package(Git REQUIRED)
message(STATUS "Project name: ${PROJECT_NAME}")
message(STATUS "Project version: ${PROJECT_VERSION_STRING_FULL}")

message("====================================")
message("\tBUILD MODE: ${CMAKE_BUILD_TYPE}")
message("\tC++ compiler: ${CMAKE_CXX_COMPILER}")
message("\tC compiler: ${CMAKE_C_COMPILER}")
message("====================================")

find_package(OpenMP)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CARAMEL_COMPILE_OPTIONS
    -Wall
    -Wextra
    # -Werror
    -Wno-unused-function
    # -Wno-ignored-optimization-argument
    -Wno-psabi
    -pedantic
    $<$<CONFIG:Debug>:-Og>
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
    $<$<CONFIG:DebugWithAsan>:-Og>
    $<$<CONFIG:DebugWithAsan>:-g>
    $<$<CONFIG:DebugWithAsan>:-fno-omit-frame-pointer>
    $<$<CONFIG:Release>:-DNDEBUG>
    $<$<CONFIG:Release>:-Ofast>
    $<$<CONFIG:Release>:-funroll-loops>
    $<$<CONFIG:Release>:-ftree-vectorize>
    $<$<CONFIG:RelWithDebInfo>:-DNDEBUG>
    $<$<CONFIG:RelWithDebInfo>:-Ofast>
    $<$<CONFIG:RelWithDebInfo>:-funroll-loops>
    $<$<CONFIG:RelWithDebInfo>:-ftree-vectorize>
    $<$<CONFIG:RelWithDebInfo>:-g>
    $<$<CONFIG:RelWithDebInfo>:-fno-omit-frame-pointer>)

set(CARAMEL_ASAN_COMPILE_OPTIONS $<$<CONFIG:DebugWithAsan>:-fsanitize=address>)

# PERFORMANCE_COMPARISON is always enabled by default in cereal CMakeLists which
# requires the boost and we dont want to add the boost dependencies, hence
# skipping the performance comparison.
set(SKIP_PERFORMANCE_COMPARISON ON)
add_subdirectory(deps/cereal EXCLUDE_FROM_ALL)

add_subdirectory(deps/pybind11 EXCLUDE_FROM_ALL)
add_subdirectory(deps/googletest EXCLUDE_FROM_ALL)

include_directories(deps)

find_package(Python3 COMPONENTS Interpreter Development)

# Enable testing
include(CTest)

include_directories(".")
include_directories(${CMAKE_BINARY_DIR})

add_subdirectory(src)
add_subdirectory(tests)

set(CARAMEL_SOURCES
    ${PROJECT_SOURCE_DIR}/src/Modulo2System.cc
    ${PROJECT_SOURCE_DIR}/src/BitArray.cc
    ${PROJECT_SOURCE_DIR}/src/construct/Codec.cc
    ${PROJECT_SOURCE_DIR}/src/construct/BucketedHashStore.cc
    ${PROJECT_SOURCE_DIR}/src/construct/SpookyHash.cc
    ${PROJECT_SOURCE_DIR}/src/solve/GaussianElimination.cc
    ${PROJECT_SOURCE_DIR}/src/solve/HypergraphPeeler.cc
    ${PROJECT_SOURCE_DIR}/src/solve/LazyGaussianElimination.cc
    ${PROJECT_SOURCE_DIR}/src/solve/Solve.cc
)
add_library(caramel_lib STATIC ${CARAMEL_SOURCES})
target_link_libraries(caramel_lib PUBLIC OpenMP::OpenMP_CXX cereal::cereal)

pybind11_add_module(_caramel ${PROJECT_SOURCE_DIR}/python_bindings/PythonBindings.cc)
target_link_libraries(_caramel PUBLIC caramel_lib)

target_compile_options(caramel_lib PRIVATE ${CARAMEL_COMPILE_OPTIONS})
target_compile_options(_caramel PRIVATE ${CARAMEL_COMPILE_OPTIONS})

target_compile_options(caramel_lib PUBLIC ${CARAMEL_ASAN_COMPILE_OPTIONS})

target_link_options(caramel_lib PUBLIC ${CARAMEL_ASAN_COMPILE_OPTIONS})

target_compile_options(_caramel PRIVATE ${CARAMEL_ASAN_COMPILE_OPTIONS})

target_link_options(_caramel PRIVATE ${CARAMEL_ASAN_COMPILE_OPTIONS})

message("===================================================")
message("\tProject Directory = ${PROJECT_SOURCE_DIR}")
message("\tC++ Compiler = ${CMAKE_CXX_COMPILER}")
message("\tSource Files = ")

foreach(SOURCE_FILE IN LISTS CPP_SOURCE)
    message("\t ${SOURCE_FILE}")
endforeach(SOURCE_FILE IN LISTS CPP_SOURCE)

