#!/bin/bash
if [ "$1" = "float" ]; then
   filename=test-fadd
elif [ "$1" = "fix" ]; then
   filename=test-tsc
elif [ "$1" = "max" ]; then
   filename=test-Tmax
elif [ "$1" = "rand" ];then
   filename=test-Rand
elif [ "$1" = "load" ];then
   filename=test-load
elif [ "$1" = "loadc" ];then
   filename=test-loadc
elif [ "$1" = "loadse" ];then
   filename=test-loadse
elif [ "$1" = "lorder" ];then
   filename=test-loadwithorder
elif [ "$1" = "lmix" ];then
   filename=test-loadwithmix
elif [ "$1" = "lrand" ];then
   filename=test-loadwithrand
elif [ "$1" = "lcorder" ];then
   filename=test-loadcwithorder
elif [ "$1" = "lcmix" ];then
   filename=test-loadcwithmix
elif [ "$1" = "lcrand" ];then
   filename=test-loadcwithrand
else
   echo "wrong para"
   exit
fi

test -d $filename
stat=$?

if [ $stat -eq 1 ];then
   mkdir $filename
fi

cd $filename
clang-3.5 -S -emit-llvm -I.. ../../test/${filename}.c -o ${filename}.ll

#llvm-as-3.5 ${filename}.ll -o ${filename}.bc
opt-3.5 -load ../src/libLLVMPred.so -InstTiming ${filename}.ll -S -o ${filename}new.ll
#llvm-dis-3.5 ${filename}new.bc -o ${filename}new.ll
clang-3.5 -S  ${filename}new.ll -o ${filename}new.s
clang-3.5 -O0 ${filename}new.ll -o test

#i=0
#while [ $i -lt 1000 ];
#do
#   ./test >> ${filename}.data
#   let i++
#done

