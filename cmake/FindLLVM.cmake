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
# LLVM_LIBRARIES    - A list of libraries which should be linked against.
# 
# Using Following macros to set library:
# llvm_map_components_to_libraries
#
if(NOT DEFINED ${LLVM_RECOMMAND_VERSION})
    SET(LLVM_RECOMMAND_VERSION 3.2)
endif()

if(NOT DEFINED ${LLVM_ROOT})
  # find llvm-config. perfers to the one with version suffix, Ex:llvm-config-3.2
  find_program(LLVM_CONFIG_EXE NAMES "llvm-config-${LLVM_RECOMMAND_VERSION}" "llvm-config")

  if(NOT LLVM_CONFIG_EXE)
      set(LLVM_FOUND False)
      message(FATAL_ERROR "Not Found LLVM")
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
_llvm_config(LLVM_C_FLAGS --cflags)
_llvm_config(LLVM_CPP_FLAGS --cppflags)
_llvm_config(LLVM_CXX_FLAGS --cxxflags)
_llvm_config(LLVM_LD_FLAGS --ldflags)
string(REGEX MATCH "-l.*" LLVM_DEPEND_LIBRARIES ${LLVM_LD_FLAGS})
message(STATUS "${LLVM_DEPEND_LIBRARIES}")
message(STATUS "${LLVM_LD_FLAGS}")

macro(llvm_map_components_to_libraries _var_name)
    _llvm_config(${_var_name} --libs "${ARGN}")
    #string(REPLACE "-l" "" ${_var_name} "${${_var_name}}")
endmacro()

message(STATUS "Found LLVM Version ${LLVM_VERSION} ")

