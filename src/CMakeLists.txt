
include_directories(
	${LLVM_PROF_INCLUDE_DIRS}
	${PROJECT_BINARY_DIR}
	${LLVM_INCLUDE_DIRS} 
	)
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS} -Wall --std=c++11)


llvm_map_components_to_libraries(LLVM_PRED_LIBRARY bitreader analysis)

add_executable(llvm-prob
    llvm-prob.cpp
    )

target_link_libraries(llvm-prob
    ${LLVM_PRED_LIBRARY}
    ${LLVM_LIBRARIES}
    )

llvm_map_components_to_libraries(LLVM_LOOP_LIBRARY bitreader analysis bitwriter)

add_executable(llvm-loop
    llvm-loop.cpp
	loop.cpp
	debug.cpp
    )

target_link_libraries(llvm-loop
	${LLVM_PROF_LIBRARIES}
	${LLVM_DYNAMIC_LIBRARY}
    ${LLVM_LIBRARIES}
    )

install(TARGETS llvm-loop DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
