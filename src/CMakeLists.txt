
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

add_library(LLVMPred SHARED
	KnownLibCallInfo.cpp
	loop.cpp
	util.cpp
	)

target_link_libraries(LLVMPred
	${LLVM_DYNAMIC_LIBRARY}
	${LLVM_PROF_LIBRARIES}
	)

add_executable(llvm-loop
	llvm-loop.cpp
	)

target_link_libraries(llvm-loop
	${LLVM_LIBRARIES}
	LLVMPred
	)

add_executable(edge-merge
	edge-merge.cpp
	)


install(TARGETS llvm-loop DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
