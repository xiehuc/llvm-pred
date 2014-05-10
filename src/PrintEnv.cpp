#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>

namespace lle{
   class PrintEnv;
}

using namespace lle;
using namespace llvm;
/* A simple helper to print environment parameters
 * useage:
 *    opt -load src/libLLVMPred.so -print-env --disable-output <bitcode>
 */
class lle::PrintEnv: public ModulePass
{
   public:
   static char ID;
   PrintEnv():ModulePass(ID) {}
   bool runOnModule(Module& M);
};

char PrintEnv::ID = 0;

static RegisterPass<PrintEnv> X("print-env","print environment params");

bool PrintEnv::runOnModule(Module& M)
{
#define printenv(env) errs()<<env<<":   "<<(getenv(env)?:"")<<"\n";
   printenv("SHRINK_LEVEL");
   printenv("LIBCALL_FILE");
   printenv("IGNOREFUNC_FILE");
   return false;
}

