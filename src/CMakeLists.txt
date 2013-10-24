
include_directories(${LLVM_INCLUDE_DIRS} ${PROJECT_BINARY_DIR})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS} --std=c++11)


llvm_map_components_to_libraries(LLVM_PRED_LIBRARY bitreader analysis)

add_executable(llvm-pred
	llvm-pred.cpp
	)

target_link_libraries(llvm-pred
	${LLVM_PRED_LIBRARY}
	pthread
	dl
	)

