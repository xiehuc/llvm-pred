include_directories(${LLVM_INCLUDE_DIRS} ${PROJECT_BINARY_DIR})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS} --std=c++11)

if(LLVM_VERSION STREQUAL "3.2")
llvm_map_components_to_libraries(LLVM_IRREADER_LIBRARY bitreader asmparser)
elseif(LLVM_VERSION STREQUAL "3.3")
llvm_map_components_to_libraries(LLVM_IRREADER_LIBRARY irreader)
else()
llvm_map_components_to_libraries(LLVM_IRREADER_LIBRARY irreader)
endif()
llvm_map_components_to_libraries(LLVM_JIT_LIBRARY jit x86)
add_executable(irread 1.irread.cpp)
target_link_libraries(irread 
    ${LLVM_IRREADER_LIBRARY}
    ${LLVM_LIBRARIES}
    )

add_executable(jit 2.jit.cpp)
target_link_libraries(jit 
    ${LLVM_IRREADER_LIBRARY}
    ${LLVM_JIT_LIBRARY}
    ${LLVM_LIBRARIES}
    rt
    )

