#!/bin/bash
#quick make a bitcode to loader
#usage ./quick-make [options] <bitcode>

OPTS=$(getopt -o ecdh --long "echo,cgpop,edge,help" -n $(basename "$0") -- "$@")

eval set -- "$OPTS"
FRONT=
LFLAGS=
EDGE=0
CGPOP=0

function print_help
{
   echo "$0 [options] <bitcode>"
   echo "options:"
   echo "   --echo: echo all command"
   echo "   --edge: used for compile edge profiling"
   echo "   --help: show this help"
   echo "   --cgpop: use for compile cgpop"
   exit 0
}

while true; do
   case "$1" in
      --echo) FRONT=echo; shift ;;
      --edge) EDGE=1; shift ;;
      --cgpop) 
         CGPOP=1; 
         LFLAGS="-lnetcdf -lnetcdff" 
         shift ;;
      --help) print_help; shift;;
      --) shift; break ;;
      *) print_help ;;
   esac
done

input="$1"
name=$(basename $1)
name=${name%.bc}

if [ "$EDGE" -eq "1" ]; then
$FRONT opt -load src/libLLVMPred.so -insert-edge-profiling $input -o /tmp/$name.e.ll -S
suffix="e"
else
$FRONT opt -load src/libLLVMPred.so -PerfPred -insert-pred-profiling -insert-mpi-profiling $input -o /tmp/$name.1.ll -S
$FRONT opt -load src/libLLVMPred.so -Reduce /tmp/$name.1.ll -o /tmp/$name.2.ll -S
if [ "$CGPOP" -eq "1" ]; then
$FRONT opt -load src/libLLVMPred.so -Force -Reduce /tmp/$name.2.ll -o /tmp/$name.3.ll -S
suffix="3"
else
suffix="2"
fi
fi
$FRONT clang /tmp/$name.$suffix.ll -o /tmp/$name.$suffix.o -c
$FRONT gfortran /tmp/$name.$suffix.o -o $name.$suffix $LFLAGS `pkg-config llvm-prof --variable=profile_rt_lib`
