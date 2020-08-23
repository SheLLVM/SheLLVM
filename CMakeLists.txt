cmake_minimum_required(VERSION 3.4)
set(CMAKE_DISABLE_SOURCE_CHANGES ON) # Must go before project() below
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON) # Must go before project() below

if(NOT DEFINED CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER "clang")
endif()
if(NOT DEFINED CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER "clang++")
endif()
project(SheLLVM)

if(NOT (CMAKE_C_COMPILER_ID MATCHES "Clang" AND
        CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
  message(FATAL_ERROR "Can only work with Clang!")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_TESTS "Build the various test cases" OFF)

add_subdirectory(llvm)
if(ENABLE_TESTS)
  add_subdirectory(tests)
endif()
