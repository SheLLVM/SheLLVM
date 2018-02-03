cmake_minimum_required(VERSION 3.1)
set(CMAKE_DISABLE_SOURCE_CHANGES ON) # Must go before project() below
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON) # Must go before project() below
project(SheLLVM)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(llvm)
