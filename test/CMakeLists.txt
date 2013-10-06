include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS} --std=c++11)

llvm_map_components_to_libraries(LLVM_IRREADER_LIRARY irreader)
llvm_map_components_to_libraries(LLVM_JIT_LIRARY jit x86)

add_executable(irread 1.irread.cpp)
target_link_libraries(irread 
    ${LLVM_DEPEND_LIBRARIES}
    ${LLVM_IRREADER_LIRARY}
    )

add_executable(jit 2.jit.cpp)
target_link_libraries(jit 
    ${LLVM_DEPEND_LIBRARIES}
    ${LLVM_IRREADER_LIRARY}
    ${LLVM_JIT_LIRARY}
    )

