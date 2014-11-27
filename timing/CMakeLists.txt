
include_directories(
	${PROJECT_BINARY_DIR}
	)
add_library(timing STATIC
   timing_source.c
   libtiming.c
   )
