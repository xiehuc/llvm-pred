#!/bin/bash
# a script to help compare two directories llvmprof.out file

if [ $# -ne 2 ] 
then 
   echo "useage: $0 dir1 dir2"
   exit
fi

lhs=(`ls -1 --color=never $1`)
rhs=(`ls -1 --color=never $2`)

if [ ${#lhs[@]} -ne ${#rhs[@]} ]
then
   echo 'hello'
   echo "$1 file numbers not match with $2"
   exit
fi

sz=${#lhs[@]}
echo "$1 <---> $2"
for (( i=0 ; i<$sz ; i++ ))
do
   echo "${lhs[$i]} <---> ${rhs[$i]} :"
   llvm-prof -diff $1/${lhs[$i]} $2/${rhs[$i]}
done

