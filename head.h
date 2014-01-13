#include "config.h"

#if LLVM_VERSION_MAJOR == 3 
#if LLVM_VERSION_MINOR == 2
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Support/IRReader.h>
#elif LLVM_VERSION_MINOR >= 3
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#endif
#endif

#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/InstIterator.h>
