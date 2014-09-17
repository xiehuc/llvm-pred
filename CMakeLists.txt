cmake_minimum_required(VERSION 2.8)
project(llvm-pred)

set(CMAKE_MODULE_PATH 
   ${CMAKE_MODULE_PATH}
   ${CMAKE_SOURCE_DIR}/cmake
   )

if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
endif()

set(LLVM_RECOMMAND_VERSION "" CACHE STRING "Switch the llvm version")
set_property(CACHE LLVM_RECOMMAND_VERSION PROPERTY STRINGS "" "3.4")
find_package(LLVM)

option(DYNAMIC_LINK "Link Dynamic LLVM Libraries" ${LLVM_DYNAMIC_LIBRARY_FOUND})
option(WITH_DEBUG "Enable Extra Debug Output" OFF)

find_package(PkgConfig)
pkg_check_modules(LLVM_PROF llvm-prof REQUIRED)
pkg_check_modules(GSL gsl REQUIRED)

if(NOT WITH_DEBUG)
   set(NO_DEBUG On)
endif()
configure_file(config.h.in config.h)

add_subdirectory(src)
