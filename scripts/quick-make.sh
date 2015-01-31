#!/bin/bash
#quick make a bitcode to loader
#usage ./quick-make <bitcode>
name=$(basename $1)
name=${name%.bc}
if [ $# -ge 2 ]; then
   if [ "$2" == "edge" ]; then
      opt -load src/libLLVMPred.so -insert-edge-profiling $1 -o /tmp/$name.e.ll -S
      clang /tmp/$name.e.ll -o /tmp/$name.e.o -c; mpif90 /tmp/$name.e.o -o $name.e `pkg-config llvm-prof --variable=profile_rt_lib`
      exit
   elif [ "$2" == "cgpop" ]; then
      opt -load src/libLLVMPred.so -PerfPred -insert-pred-profiling -insert-mpi-profiling $1 -o /tmp/$name.1.ll -S
      opt -load src/libLLVMPred.so -Reduce /tmp/$name.1.ll -o /tmp/$name.2.ll -S
      opt -load src/libLLVMPred.so -Force -Reduce /tmp/$name.2.ll -o /tmp/$name.3.ll -S
      clang /tmp/$name.3.ll -o /tmp/$name.3.o -c; gfortran /tmp/$name.3.o -o $name.3 -lnetcdf -lnetcdff `pkg-config llvm-prof --variable=profile_rt_lib`
      exit
   fi
fi
opt -load src/libLLVMPred.so -PerfPred -insert-pred-profiling -insert-mpi-profiling $1 -o /tmp/$name.1.ll -S
opt -load src/libLLVMPred.so -Reduce /tmp/$name.1.ll -o /tmp/$name.2.ll -S
clang /tmp/$name.2.ll -o /tmp/$name.2.o -c; gfortran /tmp/$name.2.o -o $name.2 `pkg-config llvm-prof --variable=profile_rt_lib`
