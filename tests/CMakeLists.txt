add_custom_target(flatten-basic ALL
  COMMAND ${CMAKE_C_COMPILER} -O1 -Xclang -disable-llvm-passes
    -c -emit-llvm -o flatten-basic.bc ${CMAKE_CURRENT_SOURCE_DIR}/flatten/basic.c
  COMMAND opt -load=$<TARGET_FILE:shellvm>
    -shellvm-prepare -shellvm-flatten -shellvm-global2stack -mem2reg -globaldce
    -o flatten-basic-out.bc flatten-basic.bc
  BYPRODUCTS flatten-basic.bc flatten-basic-out.bc
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
