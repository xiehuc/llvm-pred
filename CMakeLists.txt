cmake_minimum_required(VERSION 2.8)
project(llvm-pred)

option(ENABLE_DEBUG "Produce lots of debug information " OFF)

set(CMAKE_MODULE_PATH 
    ${CMAKE_MODULE_PATH}
    ${CMAKE_SOURCE_DIR}/cmake
    )

set(LLVM_RECOMMAND_VERSION "" CACHE STRING "Switch the llvm version")
set_property(CACHE LLVM_RECOMMAND_VERSION PROPERTY STRINGS "" "3.3" "3.4")
find_package(LLVM)

option(DYNAMIC_LINK "Link Dynamic LLVM Libraries" ${LLVM_DYNAMIC_LIBRARY_FOUND})

find_package(PkgConfig)
pkg_check_modules(LLVM_PROF llvm-prof REQUIRED)

configure_file(config.h.in config.h)

add_subdirectory(src)
add_subdirectory(test)
