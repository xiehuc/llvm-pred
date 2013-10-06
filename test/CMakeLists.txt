include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS} --std=c++11)

llvm_map_components_to_libraries(LLVM_IRREADER_LIRARY irreader)

add_executable(irread 1.irread.cpp)
message(STATUS ${LLVM_IRREADER_LIRARY})
target_link_libraries(irread 
    ${LLVM_DEPEND_LIBRARIES}
    ${LLVM_IRREADER_LIRARY})

