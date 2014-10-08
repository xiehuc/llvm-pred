#!/bin/sh

if [ $# -le 1 ];then
   echo "usage: $0 <bitcode> [compile flags]"
fi

bitcode=$1;shift

llc -filetype=obj $bitcode -o /tmp/$bitcode.o
mpif90 /tmp/$bitcode.o -o ${bitcode/%.bc/} $*
