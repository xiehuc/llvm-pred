#!/bin/bash
#quick make a bitcode to loader
#usage ./quick-make <bitcode>
name=$(basename $1)
name=${name%.bc}
opt -load src/libLLVMPred.so -PerfPred -insert-pred-profiling -insert-mpi-profiling $1 -o /tmp/$name.1.ll -S
opt -load src/libLLVMPred.so -Reduce /tmp/$name.1.ll -o /tmp/$name.2.ll -S
clang /tmp/$name.2.ll -o /tmp/$name.2.o -c; gfortran /tmp/$name.2.o -o $name.2 `pkg-config llvm-prof --variable=profile_rt_lib`
