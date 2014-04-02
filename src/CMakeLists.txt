
include_directories(
	${LLVM_PROF_INCLUDE_DIRS}
	${PROJECT_BINARY_DIR}
	${LLVM_INCLUDE_DIRS} 
	)
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(-fno-rtti)
add_definitions(${LLVM_DEFINITIONS} -Wall --std=c++11)


llvm_map_components_to_libraries(LLVM_PRED_LIBRARY bitreader analysis)

add_executable(llvm-prob
    llvm-prob.cpp
    )

target_link_libraries(llvm-prob
    ${LLVM_PRED_LIBRARY}
    ${LLVM_LIBRARIES}
    )

if(DYNAMIC_LINK)
	set(LLVM_LOOP_LIBRARY ${LLVM_DYNAMIC_LIBRARY})
else()
	llvm_map_components_to_libraries(LLVM_LOOP_LIBRARY bitreader analysis bitwriter)
endif()

add_executable(llvm-loop
	llvm-loop.cpp
	display.cpp
	loop.cpp
	)

target_link_libraries(llvm-loop
	${LLVM_PROF_LIBRARIES}
	${LLVM_LOOP_LIBRARY}
	${LLVM_LIBRARIES}
	)

add_executable(edge-merge
	edge-merge.cpp
	)

add_library(KnownLibCall SHARED
	KnownLibCallInfo.cpp
	)

target_link_libraries(KnownLibCall
	${LLVM_DYNAMIC_LIBRARY}
	)

install(TARGETS llvm-loop DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
