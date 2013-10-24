cmake_minimum_required(VERSION 2.8)
project(llvm-pred)

set(CMAKE_MODULE_PATH 
    ${CMAKE_MODULE_PATH}
    ${CMAKE_SOURCE_DIR}/cmake
    )

find_package(LLVM)

configure_file(config.h.in config.h)
configure_file(head.h head.h COPYONLY)

add_subdirectory(test)
add_subdirectory(src)
