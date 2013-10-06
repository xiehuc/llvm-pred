cmake_minimum_required(VERSION 2.8)
project(llvm-pred)

set(CMAKE_MODULE_PATH 
    ${CMAKE_MODULE_PATH}
    ${CMAKE_SOURCE_DIR}/cmake
    )
set(CMAKE_PREFIX_PATH
    ${CMAKE_PREFIX_PATH}
    ${CMAKE_SOURCE_DIR}/cmake
    )

find_package(LLVM)

add_subdirectory(test)
