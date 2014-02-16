#include <llvm/IR/Value.h>
#include <llvm/Support/raw_ostream.h>

#define DEBUG(expr) expr
#define VERBOSE(expr,verb) (expr || (outs()<<"<<HERE>>:"<<*verb<<"\n",0))

extern llvm::raw_ostream& out_stream;
