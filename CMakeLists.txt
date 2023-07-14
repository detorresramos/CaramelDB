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

# Project versioning
find_package(Git REQUIRED)
message(STATUS "Project name: ${PROJECT_NAME}")
message(STATUS "Project version: ${PROJECT_VERSION_STRING_FULL}")

message("====================================")
message("\tBUILD MODE: ${CMAKE_BUILD_TYPE}")
message("\tC++ compiler: ${CMAKE_CXX_COMPILER}")
message("\tC compiler: ${CMAKE_C_COMPILER}")
message("====================================")

# Add OpenMP
find_package(OpenMP)

if(NOT OpenMP_FOUND)
    message("OpenMP Not Found. Building the Package using LLVM's OpenMP.")
    set(LLVM_OPENMP_VERSION "13.0.1")
    set(LLVM_OPENMP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/openmp")

    if(NOT EXISTS ${LLVM_OPENMP_SOURCE_DIR})
        message("Downloading openmp-${LLVM_OPENMP_VERSION} into ${LLVM_OPENMP_SOURCE_DIR}")
        set(LLVM_OPENMP_DOWNLOAD_URL "https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_OPENMP_VERSION}/openmp-${LLVM_OPENMP_VERSION}.src.tar.xz")

        file(
            DOWNLOAD
            ${LLVM_OPENMP_DOWNLOAD_URL}
            "${CMAKE_CURRENT_BINARY_DIR}/openmp-${LLVM_OPENMP_VERSION}.tar.bz2"
        )
        file(ARCHIVE_EXTRACT INPUT
            "${CMAKE_CURRENT_BINARY_DIR}/openmp-${LLVM_OPENMP_VERSION}.tar.bz2"
            DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
        )
        file(RENAME
            "${CMAKE_CURRENT_BINARY_DIR}/openmp-${LLVM_OPENMP_VERSION}.src"
            "${LLVM_OPENMP_SOURCE_DIR}"
        )
    endif()

    set(OPENMP_STANDALONE_BUILD ON)
    add_subdirectory(${LLVM_OPENMP_SOURCE_DIR} EXCLUDE_FROM_ALL)
    add_library(OpenMP::OpenMP_CXX ALIAS omp)
endif(NOT OpenMP_FOUND)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Add compiler flags that should be used - Wall : enable all warnings - O3:
# enable all optimizations.
# TODO(any): use other compile flags and modes
set(CMAKE_CXX_FLAGS "-O3 -Wall -Wextra -Werror -Wno-unused-function -Wno-ignored-optimization-argument -Wno-psabi -pedantic")

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
    ${PROJECT_SOURCE_DIR}/src/Dummy.cc
    ${PROJECT_SOURCE_DIR}/src/Modulo2System.cc
    ${PROJECT_SOURCE_DIR}/src/BitArray.cc
    ${PROJECT_SOURCE_DIR}/src/solve/GaussianElimination.cc
    ${PROJECT_SOURCE_DIR}/src/solve/LazyGaussianElimination.cc
)
add_library(caramel_lib STATIC ${CARAMEL_SOURCES})
target_link_libraries(caramel_lib PUBLIC OpenMP::OpenMP_CXX)

pybind11_add_module(caramel ${PROJECT_SOURCE_DIR}/python_bindings/PythonBindings.cc)
target_link_libraries(caramel PUBLIC caramel_lib)

message("===================================================")
message("\tProject Directory = ${PROJECT_SOURCE_DIR}")
message("\tC++ Compiler = ${CMAKE_CXX_COMPILER}")
message("\tSource Files = ")

foreach(SOURCE_FILE IN LISTS CPP_SOURCE)
    message("\t ${SOURCE_FILE}")
endforeach(SOURCE_FILE IN LISTS CPP_SOURCE)
