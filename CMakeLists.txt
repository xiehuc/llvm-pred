cmake_minimum_required(VERSION 2.8)
project(llvm-pred)

set(CMAKE_MODULE_PATH 
    ${CMAKE_MODULE_PATH}
    ${CMAKE_SOURCE_DIR}/cmake
    )

set(LLVM_RECOMMAND_VERSION "" CACHE STRING "Switch the llvm version")
set_property(CACHE LLVM_RECOMMAND_VERSION PROPERTY STRINGS "" "3.2" "3.3")

find_package(LLVM)

configure_file(config.h.in config.h)
configure_file(head.h head.h COPYONLY)

add_subdirectory(test)
add_subdirectory(src)
