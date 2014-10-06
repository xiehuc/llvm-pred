#!/usr/bin/env bash
# a helper script used to convert a dir's BLOCK_FREQ predication value
# profiling to edge profiling

if [ $# -le 2 ]; then
   echo "usage: $0 <.so> <input.bc> [llvmprof.out]..."
   echo "example: $0 <.so> <input.bc> llvmprof.out.*"
   exit -1
fi

load=$1; shift
input=$1; shift
for i in "$@"
do
   opt -load $load -profile-info-file=$i -profile-loader -profile-info-output=${i/out/edge} -Value-To-Edge $input -o /dev/null
done
