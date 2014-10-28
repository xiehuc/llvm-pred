cmake_minimum_required(VERSION 2.8)
project(llvm-pred)

set(CMAKE_MODULE_PATH 
   ${CMAKE_MODULE_PATH}
   ${CMAKE_SOURCE_DIR}/cmake
   )

if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
endif()

find_package(LLVM)

option(DYNAMIC_LINK "Link Dynamic LLVM Libraries" ${LLVM_DYNAMIC_LIBRARY_FOUND})
option(WITH_DEBUG "Enable Extra Debug Output" On)
set(PRED_TYPE "BLOCK_FREQ" CACHE STRING "Select What to Predicate for Performance Model")
set_property(CACHE PRED_TYPE PROPERTY STRINGS "BLOCK_FREQ" "EXEC_TIME")
set(TIMING "TSC" CACHE STRINGS "Select timing implement")
set_property(CACHE TIMING PROPERTY STRINGS "TSC" "CLOCK_GETTIME")

find_package(PkgConfig)
pkg_check_modules(LLVM_PROF llvm-prof REQUIRED)
pkg_check_modules(GSL gsl REQUIRED)

if(NOT WITH_DEBUG)
   set(NO_DEBUG On)
endif()
configure_file(config.h.in config.h)

add_subdirectory(src)
