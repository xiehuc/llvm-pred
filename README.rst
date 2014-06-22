=========
llvm-pred
=========

A set of Pass and functions to help analysis program's performance model.

build
------

if gcc doesn't support c++11 ,can use clang

.. code:: bash

    $ export CC=clang
    $ export CXX=clang++
    $ mkdir build;cd build
    $ cmake .. -DLLVM_RECOMMAND_VERSION="3.4"
    $ make

use ``LLVM_RECOMMAND_VERSION`` to change llvm version directly

script
-------

drawline.py : used to draw lines from value profiling
dirdiff.sh  : compare two dir's llvmprof.out file and report whether they are
same

performance analysis based dynamic information
-----------------------------------------------

1. insert edge profiling into bitcode.
2. compile program and run.
3. output the text format profiling log using ``llvm-prof``
4. merge multi process's log into one. using ``edge-merge``
5. curfit ??? ``curve-fit``
