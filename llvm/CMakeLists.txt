find_package(LLVM REQUIRED)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
add_library(shellvm MODULE
  PreparePass.cpp
  PrecheckPass.cpp
  MergeCallsPass.cpp
  FlattenPass.cpp
  GlobalToStackPass.cpp
  InlineCtorsPass.cpp
  PostcheckPass.cpp
)

set_target_properties(shellvm PROPERTIES PREFIX "")
