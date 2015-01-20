#! /bin/bash
if [[ $1 = "del" ]]; then
   rm $2.ll
   rm $2tmp.ll
   rm $2tmp
   echo "delete over!"
fi
if [[ $1 != del ]]; then
   clang -S -c -emit-llvm $1.c -o $1.ll
   opt-3.5 -S -load ../build/src/libLLVMPred.so -InstTiming $1.ll -o $1tmp.ll
   clang -lm -O0 $1tmp.ll -o $1tmp
fi
