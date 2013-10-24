# This file provides information and services to the final user.


if(NOT DEFINED ${LLVM_RECOMMAND_VERSION})
    SET(LLVM_RECOMMAND_VERSION 3.2)
endif()

if(NOT DEFINED ${LLVM_ROOT})
  # find llvm-config. perfers to the one with version suffix, Ex:llvm-config-3.2
  find_program(LLVM_CONFIG_EXE NAMES "llvm-config-${LLVM_RECOMMAND_VERSION}" "llvm-config")

  if(NOT LLVM_CONFIG_EXE)
      set(LLVM_FOUND False)
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

_llvm_config(LLVM_VERSION --version)
set(LLVM_PACKAGE_VERSION ${LLVM_VERSION})

#set(LLVM_VERSION_MAJOR @LLVM_VERSION_MAJOR@)
#set(LLVM_VERSION_MINOR @LLVM_VERSION_MINOR@)

#set(LLVM_COMMON_DEPENDS @LLVM_COMMON_DEPENDS@)

#set_property( GLOBAL PROPERTY LLVM_LIBS "@llvm_libs@")

#set(LLVM_ALL_TARGETS @LLVM_ALL_TARGETS@)
#
_llvm_config(_LLVM_TARGETS_TO_BUILD --components)
string(REPLACE " " ";" LLVM_TARGETS_TO_BUILD ${_LLVM_TARGETS_TO_BUILD})
list(LENGTH LLVM_TARGETS_TO_BUILD len)
list(FIND LLVM_TARGETS_TO_BUILD "irreader" len)
message(STATUS "len = ${len}")
#
#set(LLVM_TARGETS_WITH_JIT @LLVM_TARGETS_WITH_JIT@)
#
#@all_llvm_lib_deps@
#
#set(TARGET_TRIPLE "@TARGET_TRIPLE@")
#
#set(LLVM_TOOLS_BINARY_DIR @LLVM_TOOLS_BINARY_DIR@)
#
#set(LLVM_ENABLE_THREADS @LLVM_ENABLE_THREADS@)
#
#set(LLVM_ENABLE_ZLIB @LLVM_ENABLE_ZLIB@)
#
#set(LLVM_NATIVE_ARCH @LLVM_NATIVE_ARCH@)
#
#set(LLVM_ENABLE_PIC @LLVM_ENABLE_PIC@)
#
#set(HAVE_LIBDL @HAVE_LIBDL@)
#set(HAVE_LIBPTHREAD @HAVE_LIBPTHREAD@)
#set(HAVE_LIBZ @HAVE_LIBZ@)
#set(LLVM_ON_UNIX @LLVM_ON_UNIX@)
#set(LLVM_ON_WIN32 @LLVM_ON_WIN32@)
#
set(LLVM_INSTALL_PREFIX  ${LLVM_ROOT})
set(LLVM_INCLUDE_DIRS ${LLVM_INSTALL_PREFIX}/include)
set(LLVM_LIBRARY_DIRS ${LLVM_INSTALL_PREFIX}/lib)
set(LLVM_DEFINITIONS "-D__STDC_LIMIT_MACROS" "-D__STDC_CONSTANT_MACROS")

# We try to include using the current setting of CMAKE_MODULE_PATH,
# which suppossedly was filled by the user with the directory where
# this file was installed:
include( LLVM-Config OPTIONAL RESULT_VARIABLE LLVMCONFIG_INCLUDED )

# If failed, we assume that this is an un-installed build:
#if( NOT LLVMCONFIG_INCLUDED )
#  set(CMAKE_MODULE_PATH
#    ${CMAKE_MODULE_PATH}
#    "@LLVM_SOURCE_DIR@/cmake/modules")
#  include( LLVM-Config )
#endif()

