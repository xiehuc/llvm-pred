# - Find LLVM 
# This module can be used to find LLVM.
# It requires that the llvm-config executable be available on the system path.
# Once found, llvm-config is used for everything else.
#
# The following variables are set:
#
# LLVM_FOUND                 - Set to YES if LLVM is found.
# LLVM_VERSION               - Set to the decimal version of the LLVM library.
# LLVM_INCLUDE_DIRS          - A list of directories where the LLVM headers are located.
# LLVM_LIBRARY_DIRS          - A list of directories where the LLVM libraries are located.
# LLVM_LIBRARIES             - A list of libraries which should be linked
# LLVM_DYNAMIC_LIBRARY       - A single dynamic llvm shared library
# LLVM_DYNAMIC_LIBRARY_FOUND - Whether found the dynamic llvm shared library
# 
# Using Following macros to set static library:
# llvm_map_components_to_libraries(OUTPUT_VARIABLE ${llvm components})
# 
# tutorial:
#   1.  select default LLVM version:
#       cmake .. -DLLVM_RECOMMEND_VERSION="3.5"
#   2.  set include dir and link dir:
#       include_directories(${LLVM_INCLUDE_DIRS})
#       link_directories(${LLVM_LIBRARY_DIRS})
#   3.a link static libraries:
#       llvm_map_components_to_libraries(LLVM_IRREADER_LIRARY irreader)
#       target_link_libraries(target
#           ${LLVM_LIBRARIES}
#           ${LLVM_IRREADER_LIRARY}
#           )
#   3.b link a dynamic library:
#       target_link_libraries(target ${LLVM_DYNAMIC_LIBRARY}) 
#
# 14-10-26: 
#    LLVM_RECOMMAND_VERSION --> LLVM_RECOMMEND_VERSION
#    update tutorial
# 
# version: 0.9.1
#    add LLVM_FLAGS_NDEBUG means llvm build with NDEBUG
#
# version: 0.9
#    remove LLVM_{C/CPP/CXX}_FLAGS which import -DNDEBUG
#
#
if(NOT DEFINED LLVM_RECOMMEND_VERSION)
   set(LLVM_RECOMMEND_VERSION "" CACHE STRING "Switch the llvm version")
   set_property(CACHE LLVM_RECOMMEND_VERSION PROPERTY STRINGS "" "3.4" "3.5")
endif()


if(NOT(DEFINED LLVM_ROOT) )
	if(NOT "${LLVM_VERSION}" EQUAL "{LLVM_RECOMMEND_VERSION}")
		unset(LLVM_CONFIG_EXE CACHE)
		unset(LLVM_DYNAMIC_LIBRARY CACHE)
	endif()
	# find llvm-config. perfers to the one with version suffix, Ex:llvm-config-3.2
	find_program(LLVM_CONFIG_EXE NAMES "llvm-config-${LLVM_RECOMMEND_VERSION}" "llvm-config")

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
add_definitions(-D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS)

_llvm_config(LLVM_VERSION --version)
STRING(REGEX REPLACE "^([0-9]+)\\.[0-9]+(svn)?\\.?[0-9]*" "\\1" LLVM_VERSION_MAJOR "${LLVM_VERSION}")
STRING(REGEX REPLACE "^[0-9]+\\.([0-9]+)(svn)?\\.?[0-9]*" "\\1" LLVM_VERSION_MINOR "${LLVM_VERSION}")
_llvm_config(LLVM_LD_FLAGS --ldflags)
_llvm_config(LLVM_LIBRARY_DIRS --libdir)
_llvm_config(LLVM_INCLUDE_DIRS --includedir)
string(REGEX MATCH "-l.*" LLVM_LIBRARIES ${LLVM_LD_FLAGS})
_llvm_config(LLVM_C_FLAGS --cflags)
if(LLVM_C_FLAGS MATCHES "-DNDEBUG")
   add_definitions(-DLLVM_FLAGS_NDEBUG)
endif()

find_library(LLVM_DYNAMIC_LIBRARY 
	NAMES "LLVM" "LLVM-${LLVM_VERSION}"
   PATHS ${LLVM_LIBRARY_DIRS}
   )

if(NOT LLVM_DYNAMIC_LIBRARY)
	set(LLVM_DYNAMIC_LIBRARY_FOUND False)
else()
	set(LLVM_DYNAMIC_LIBRARY_FOUND True)
endif()

macro(llvm_map_components_to_libraries _var_name)
    _llvm_config(${_var_name} --libs "${ARGN}")
endmacro()

message(STATUS "Found LLVM Version ${LLVM_VERSION} ")

