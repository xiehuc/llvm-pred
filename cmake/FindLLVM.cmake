# - Find LLVM 
# This module can be used to find LLVM.
# It requires that the llvm-config executable be available on the system path.
# Once found, llvm-config is used for everything else.
#
# The following variables are set:
#
# LLVM_FOUND        - Set to YES if LLVM is found.
# LLVM_VERSION      - Set to the decimal version of the LLVM library.
# LLVM_C_FLAGS      - All flags that should be passed to a C compiler. 
# LLVM_CXX_FLAGS    - All flags that should be passed to a C++ compiler.
# LLVM_CPP_FLAGS    - All flags that should be passed to the C pre-processor.
# LLVM_LD_FLAGS     - Additional flags to pass to the linker.
# LLVM_LIBRARY_DIRS - A list of directories where the LLVM libraries are located.
# LLVM_INCLUDE_DIRS - A list of directories where the LLVM headers are located.
# LLVM_DEFINITIONS  - The definitions should be used
# LLVM_LIBRARIES    - A list of libraries which should be linked
# LLVM_DYNAMIC_LIBRARY - A single dynamic llvm shared library
# 
# Using Following macros to set library:
# llvm_map_components_to_libraries(OUTPUT_VARIABLE ${llvm components})
# 
# example:
# 
# include_directories(${LLVM_INCLUDE_DIRS})
# link_directories(${LLVM_LIBRARY_DIRS})
# add_definitions(${LLVM_DEFINITIONS} --std=c++11)
# 
# 
# llvm_map_components_to_libraries(LLVM_IRREADER_LIRARY irreader)
# 
# add_executable(irread 1.irread.cpp)
# target_link_libraries(target
#     ${LLVM_LIBRARIES}
#     ${LLVM_IRREADER_LIRARY}
#     )
#
if(NOT DEFINED LLVM_RECOMMAND_VERSION)
    SET(LLVM_RECOMMAND_VERSION 3.4)
endif()


#if(NOT(DEFINED LLVM_ROOT) OR NOT("${LLVM_VERSION_LAST}" VERSION_EQUAL "${LLVM_RECOMMAND_VERSION}"))
if(NOT(DEFINED LLVM_ROOT) )
	if(NOT "${LLVM_VERSION}" EQUAL "{LLVM_RECOMMAND_VERSION}")
		unset(LLVM_CONFIG_EXE CACHE)
	endif()
	# find llvm-config. perfers to the one with version suffix, Ex:llvm-config-3.2
	find_program(LLVM_CONFIG_EXE NAMES "llvm-config-${LLVM_RECOMMAND_VERSION}" "llvm-config")

	if(NOT LLVM_CONFIG_EXE)
		set(LLVM_FOUND False)
		message(FATAL_ERROR "Not Found LLVM")
	else()
		set(LLVM_FOUND True)
	endif()

	# Get the directory of llvm by using llvm-config. also remove whitespaces.
	execute_process(COMMAND ${LLVM_CONFIG_EXE} --prefix OUTPUT_VARIABLE LLVM_ROOT
		OUTPUT_STRIP_TRAILING_WHITESPACE )

endif()

macro(_llvm_config _var_name)
    execute_process(COMMAND ${LLVM_CONFIG_EXE} ${ARGN} 
        OUTPUT_VARIABLE ${_var_name}
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )
endmacro()

set(LLVM_INSTALL_PREFIX  ${LLVM_ROOT})
set(LLVM_INCLUDE_DIRS ${LLVM_INSTALL_PREFIX}/include)
set(LLVM_LIBRARY_DIRS ${LLVM_INSTALL_PREFIX}/lib)
set(LLVM_DEFINITIONS "-D__STDC_LIMIT_MACROS" "-D__STDC_CONSTANT_MACROS")

_llvm_config(LLVM_VERSION --version)
STRING(REGEX REPLACE "^([0-9]+)\\.[0-9]+(svn)?\\.?[0-9]*" "\\1" LLVM_VERSION_MAJOR "${LLVM_VERSION}")
STRING(REGEX REPLACE "^[0-9]+\\.([0-9]+)(svn)?\\.?[0-9]*" "\\1" LLVM_VERSION_MINOR "${LLVM_VERSION}")
_llvm_config(LLVM_C_FLAGS --cflags)
_llvm_config(LLVM_CPP_FLAGS --cppflags)
_llvm_config(LLVM_CXX_FLAGS --cxxflags)
_llvm_config(LLVM_LD_FLAGS --ldflags)
string(REGEX MATCH "-l.*" LLVM_LIBRARIES ${LLVM_LD_FLAGS})

find_library(LLVM_DYNAMIC_LIBRARY 
	NAMES "LLVM" "LLVM-${LLVM_VERSION}")

macro(llvm_map_components_to_libraries _var_name)
    _llvm_config(${_var_name} --libs "${ARGN}")
endmacro()

message(STATUS "Found LLVM Version ${LLVM_VERSION} ")

