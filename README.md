llvm-pred
=========

build
------

if gcc doesn't support c++11 ,can use clang

    $ export CC=clang
    $ export CXX=clang++
    $ mkdir build;cd build
    $ cmake .. -DLLVM_RECOMMAND_VERSION="3.3"
    $ make

use `LLVM_RECOMMAND_VERSION` to change llvm version directly
