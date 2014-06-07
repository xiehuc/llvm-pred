
include_directories(
	${LLVM_PROF_INCLUDE_DIRS}
	${PROJECT_BINARY_DIR}
	${LLVM_INCLUDE_DIRS} 
	)
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(-fno-rtti)
add_definitions(-Wall --std=c++11)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${LLVM_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${LLVM_CPP_FLAGS}")

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
   LoopSimplify.cpp
   LibFReshape.cpp
   SlashShrink.cpp
   Resolver.cpp
   Print.cpp
	loop.cpp
	util.cpp
   ddg.cpp
	)

target_link_libraries(LLVMPred
	${LLVM_DYNAMIC_LIBRARY}
	${LLVM_PROF_LIBRARIES}
	)

add_executable(edge-merge
	edge-merge.cpp
	)

add_executable(lle-llc
   MyAsmPrinter.cpp
   llc.cpp
   )

target_link_libraries(lle-llc
   ${LLVM_DYNAMIC_LIBRARY}
   )

#add_executable(llvm-loop
#	llvm-loop.cpp
#	)
#
#target_link_libraries(llvm-loop
#	${LLVM_LIBRARIES}
#	LLVMPred
#	)

#install(TARGETS llvm-loop DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
