# Follows instructions from this site.
# http://llvm.org/docs/CMake.html#embedding-llvm-in-your-project

# the llvm version is setting to this variable.
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

message(STATUS "Found LLVM Version ${LLVM_VERSION} ")

