
include_directories(
	${PROJECT_BINARY_DIR}
	)
add_library(libtiming STATIC
   timing_source.c
   libtiming.c
   )
