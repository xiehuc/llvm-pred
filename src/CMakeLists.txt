
include_directories(
	${LLVM_PROF_INCLUDE_DIRS}
	${PROJECT_BINARY_DIR}
	${LLVM_INCLUDE_DIRS} 
   ${PROJECT_SOURCE_DIR}/timing
	)
link_directories(
   ${LLVM_LIBRARY_DIRS} 
   ${LLVM_PROF_LIBRARY_DIRS}
   )

add_library(predicate_rt STATIC
   PredBlockProfiling.c
   )

target_link_libraries(predicate_rt
   profile_rt
   )

add_definitions(-fno-rtti)
add_definitions(-Wall --std=c++11)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNO_DEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

if(DYNAMIC_LINK)
	set(LLVM_LOOP_LIBRARY ${LLVM_DYNAMIC_LIBRARY})
else()
	llvm_map_components_to_libraries(LLVM_LOOP_LIBRARY bitreader analysis bitwriter)
endif()

add_library(LLVMPred SHARED
   PredBlockProfiling.cpp
	KnownLibCallInfo.cpp
   InsertTripCount.cpp
   BlockFreqExpr.cpp
	LoopTripCount.cpp
   DSEAdaptive.cpp
   DAEAdaptive.cpp
   LibFReshape.cpp
   PerformPred.cpp
   InstTiming.cpp
   Adaptive.cpp
   Resolver.cpp
   LockInst.cpp
   GVInfo.cpp
   Reduce.cpp
   Print.cpp
	util.cpp
   ddg.cpp
	)

target_link_libraries(LLVMPred
	${LLVM_DYNAMIC_LIBRARY}
	${LLVM_PROF_LIBRARIES}
	)

install(TARGETS LLVMPred predicate_rt DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
