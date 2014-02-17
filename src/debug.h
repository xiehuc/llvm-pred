#include <llvm/IR/Value.h>
#include <llvm/Support/raw_ostream.h>

#define DEBUG(expr) expr
#define RET_ON_FAIL(cond) DEBUG(if(!(cond)){outs()<<"Failed at:"<<__LINE__<<"\n";return NULL;})
#define VERBOSE(expr,verb) (expr || (outs()<<"<<HERE>>:"<<*verb<<"\n",0))

extern llvm::raw_ostream& out_stream;
