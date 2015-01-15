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
set(TIMING "tsc" CACHE STRINGS "Select timing implement")
set_property(CACHE TIMING PROPERTY STRINGS "tsc" "tscp" "clock_gettime")
set(TIMING_SOURCE "lmbench" CACHE STRINGS "Select cycle timging source")
set_property(CACHE TIMING_SOURCE PROPERTY STRINGS "lmbench")
set(PROMOTE_FREQ "path_prob" CACHE STRINGS "Select how to fix promote errors in performance predication")
set_property(CACHE PROMOTE_FREQ PROPERTY STRINGS "none" "select" "path_prob")
option(UNIT_TEST "Enable Build Unit Test" Off)


find_package(PkgConfig)
pkg_check_modules(LLVM_PROF llvm-prof REQUIRED)
pkg_check_modules(GSL gsl REQUIRED)

if(NOT WITH_DEBUG)
   set(NO_DEBUG On)
endif()
configure_file(config.h.in config.h)
configure_file(llvm-pred.pc.in llvm-pred.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/llvm-pred.pc DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)
message(STATUS "Add ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig to $PKG_CONFIG_PATH "
"environment variable to let pkg-config find llvm-pred.pc")

add_subdirectory(timing)
add_subdirectory(src)
if(UNIT_TEST)
add_subdirectory(unit)
endif()
