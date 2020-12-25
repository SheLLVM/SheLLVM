function(add_test_case name file)
  add_custom_target(${name} ALL
    COMMAND ${CMAKE_C_COMPILER} -O1 -Xclang -disable-llvm-passes
      -c -emit-llvm -o ${name}.bc ${CMAKE_CURRENT_SOURCE_DIR}/${file}
    COMMAND opt -load=$<TARGET_FILE:shellvm>
      -shellvm-prepare -shellvm-flatten -shellvm-global2stack
      -mem2reg -globaldce -lowerswitch
      -shellvm-postcheck
      -o ${name}-out.bc ${name}.bc
    BYPRODUCTS ${name}.bc ${name}-out.bc
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endfunction(add_test_case)

add_test_case(flatten-basic flatten/basic.c)
add_test_case(global2stack-constexprs global2stack/constexprs.c)
add_test_case(global2stack-circular global2stack/circular.c)
