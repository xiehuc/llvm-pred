
include_directories(
	${LLVM_PROF_INCLUDE_DIRS}
	${PROJECT_BINARY_DIR}
	${LLVM_INCLUDE_DIRS} 
	)
link_directories(
   ${LLVM_LIBRARY_DIRS} 
   ${LLVM_PROF_LIBRARY_DIRS}
   )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall --std=c++11 -fno-rtti")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNO_DEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

if(DYNAMIC_LINK)
	set(LLVM_LOOP_LIBRARY ${LLVM_DYNAMIC_LIBRARY})
else()
	llvm_map_components_to_libraries(LLVM_LOOP_LIBRARY bitreader analysis bitwriter)
endif()

add_library(LLVMPred SHARED
	KnownLibCallInfo.cpp
   InsertTripCount.cpp
	LoopTripCount.cpp
   DSEAdaptive.cpp
   DAEAdaptive.cpp
   PerformPred.cpp
   Adaptive.cpp
   Resolver.cpp
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

install(TARGETS LLVMPred DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
