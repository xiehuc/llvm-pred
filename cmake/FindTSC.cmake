
if(NOT DEFINED TSC_Found)

   file(STRINGS "/proc/cpuinfo" CPU_FLAGS REGEX "flags")
   list(REMOVE_DUPLICATES CPU_FLAGS)
   string(REGEX MATCH "constant_tsc" TSC_FLAGS "${CPU_FLAGS}")
   if(TSC_FLAGS)
      set(USING_TSC True)
   else()
      set(USING_CLOCK_GETTIME True)
   endif()
   set(TSC_Found True Cache BOOL)

endif()
