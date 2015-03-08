include_directories(
	${LLVM_PROF_INCLUDE_DIRS}
	${PROJECT_BINARY_DIR}
	${LLVM_INCLUDE_DIRS} 
   ${PROJECT_SOURCE_DIR}/src
	)
link_directories(
   ${LLVM_LIBRARY_DIRS} 
   ${LLVM_PROF_LIBRARY_DIRS}
   )
add_definitions(-fno-rtti)
add_definitions(-Wall --std=c++11)

add_executable(unit_test
   IgnoreListTest.cpp
   GEPFilterTest.cpp
   CGFilterTest.cpp
   NousedTest.cpp
   UtilTest.cpp
   )
target_link_libraries(unit_test
	${LLVM_DYNAMIC_LIBRARY}
   LLVMPred
   gtest
   gmock
   gmock_main
	)
