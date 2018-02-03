find_package(LLVM REQUIRED)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
add_library(shellvm MODULE
  PreparePass.cpp
  MergeCallsPass.cpp
  GlobalToStackPass.cpp)
