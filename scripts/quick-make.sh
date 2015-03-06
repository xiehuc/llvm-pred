#!/bin/bash
#quick make a bitcode to drawfcode

# process options
OPTS=$(getopt -o ecdhgl --long "echo,cgpop,edge,help,gdb,log" -n $(basename "$0") -- "$@")

eval set -- "$OPTS"
FRONT=eval
LFLAGS=
EDGE=0
CGPOP=0
ARG_BEG=
ARG_END=
MPIF90=gfortran
i=1
REDICT=

function print_help
{
   echo "$0 [options] <bitcode>"
   echo "options:"
   echo "   --log: write output to /tmp/quick-make-log/"
   echo "   --gdb: echo gdb command"
   echo "   --echo: echo all command"
   echo "   --edge: used for compile edge profiling"
   echo "   --help: show this help"
   echo "   --cgpop: use for compile cgpop"
   exit 0
}

function statement_comp
{
   echo "... statement $1 completed ..."
   return $(($1+1))
}

# process arguments
while true; do
   case "$1" in
      --log)
         mkdir -p /tmp/quick-make-log
         ARG_END+=' 2>/tmp/quick-make-log/$i.log'
         shift ;;
      --gdb) 
         FRONT=echo\ gdb
         ARG_BEG="-ex \"r"
         ARG_END="\""
         shift ;;
      --echo) FRONT=echo; shift ;;
      --edge) 
         EDGE=1; 
         MPIF90=mpif90
         shift ;;
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

# compile code
if [ "$EDGE" -eq "1" ]; then
$FRONT opt $ARG_BEG -load src/libLLVMPred.so -insert-edge-profiling $input -o /tmp/$name.e.ll -S $ARG_END
statement_comp $i; i=$?
suffix="e"
else
$FRONT opt $ARG_BEG -load src/libLLVMPred.so -PerfPred -insert-pred-profiling -insert-mpi-profiling $input -o /tmp/$name.1.ll -S $ARG_END
statement_comp $i; i=$?
$FRONT opt $ARG_BEG -load src/libLLVMPred.so -Reduce /tmp/$name.1.ll -o /tmp/$name.2.ll -S $ARG_END
statement_comp $i; i=$?
if [ "$CGPOP" -eq "1" ]; then
$FRONT opt $ARG_BEG -load src/libLLVMPred.so -Force -Reduce /tmp/$name.2.ll -o /tmp/$name.3.ll -S $ARG_END
statement_comp $i; i=$?
suffix="3"
else
suffix="2"
fi
fi
$FRONT clang /tmp/$name.$suffix.ll -o /tmp/$name.$suffix.o -c
statement_comp $i; i=$?
$FRONT $MPIF90 /tmp/$name.$suffix.o -o $name.$suffix $LFLAGS `pkg-config llvm-prof --variable=profile_rt_lib`
statement_comp $i; i=$?
