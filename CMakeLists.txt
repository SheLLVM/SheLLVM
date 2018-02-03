cmake_minimum_required(VERSION 3.0)
set(CMAKE_DISABLE_SOURCE_CHANGES ON) # Must go before project() below
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON) # Must go before project() below
project(SheLLVM)

add_subdirectory(llvm)
