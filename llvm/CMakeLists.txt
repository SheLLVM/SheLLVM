find_package(LLVM REQUIRED)

add_library(shellvm MODULE
  PreparePass.cpp
  PrecheckPass.cpp
  MergeCallsPass.cpp
  FlattenPass.cpp
  GlobalToStackPass.cpp
  InlineCtorsPass.cpp
  PostcheckPass.cpp
)
target_include_directories(shellvm PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(shellvm PRIVATE ${LLVM_DEFINITIONS})

set_target_properties(shellvm PROPERTIES PREFIX "")
