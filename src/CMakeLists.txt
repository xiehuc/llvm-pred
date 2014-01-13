
include_directories(${LLVM_INCLUDE_DIRS} ${PROJECT_BINARY_DIR})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS} --std=c++11)


llvm_map_components_to_libraries(LLVM_PRED_LIBRARY bitreader analysis)

# llvm-3.3 is last version contain llvm-prof
if(${LLVM_VERSION} VERSION_LESS "3.4")

add_executable(llvm-pred
	llvm-pred.cpp
	)

target_link_libraries(llvm-pred
	${LLVM_PRED_LIBRARY}
    ${LLVM_LIBRARIES}
	)

endif()

add_executable(llvm-prob
    llvm-prob.cpp
    )

target_link_libraries(llvm-prob
    ${LLVM_PRED_LIBRARY}
    ${LLVM_LIBRARIES}
    )

add_executable(llvm-loop
    llvm-loop.cpp
    )

target_link_libraries(llvm-loop
    ${LLVM_PRED_LIBRARY}
    ${LLVM_LIBRARIES}
    )
