#include <llvm/Pass.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/InstIterator.h>

#include <string>

#include "util.h"
#include "KnownLibCallInfo.h"

namespace lle{
   class PrintEnv;
   class PrintCgTree;
   class PrintLibCall;
}

using namespace std;
using namespace lle;
using namespace llvm;

/* A simple helper to print environment parameters
 * useage:
 *    opt -load src/libLLVMPred.so -Env /dev/null -disable-output
 */
class lle::PrintEnv: public ModulePass
{
   public:
   static char ID;
   PrintEnv():ModulePass(ID) {}
   bool runOnModule(Module& M);
};

#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR == 4
/** A simple helper to print callgraph,
 * the difference with -print-callgraph is this only print module function,
 * ant this also print a tree, insteal just print a list
 * which makes output clean and meanningful
 */
class lle::PrintCgTree: public ModulePass
{
   static void print_cg(CallGraphNode* node);
   public:
   static char ID;
   PrintCgTree():ModulePass(ID) {}
   bool runOnModule(Module& M);
};
char PrintCgTree::ID = 0;
static RegisterPass<PrintCgTree> Y("Cg", "print Callgraph Tree", true, true);
#endif
/* A simple helper to print functions called what libcalls
 * this is useful to debug and check the program structure
 */
class lle::PrintLibCall: public FunctionPass
{
   LibCallFromFile LC;
   public:
   static char ID;
   PrintLibCall():FunctionPass(ID) {};
   bool runOnFunction(Function&) ;
};

char PrintEnv::ID = 0;
char PrintLibCall::ID = 0;

static RegisterPass<PrintEnv> X("Env","print environment params", true, true);
static RegisterPass<PrintLibCall> Z("Call", "print function invokes what libcall", true, true);

bool PrintEnv::runOnModule(Module &M)
{
#define printenv(env) errs()<<env<<":   "<<(getenv(env)?:"")<<"\n";
   printenv("SHRINK_LEVEL");
   printenv("LIBCALL_FILE");
   printenv("LIBFDEF_FILE");
   printenv("IGNOREFUNC_FILE");
#undef printenv
   return false;
}

#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR == 4
bool PrintCgTree::runOnModule(Module &M)
{
   CallGraph CG;
   CG.runOnModule(M);
   CallGraphNode* root = CG.getRoot();
   errs()<<root->getFunction()->getName()<<"\n";
   print_cg(root);
   return false;
}

void PrintCgTree::print_cg(CallGraphNode *node)
{
   static vector<bool> level;
   const char* empty   = "    ";
   const char* ancient = "│   ";
   const char* parent  = "├─";
   const char* last    = "└─";
   auto lastC = node->begin();
   level.push_back(1);
   for(auto I = node->begin(), E = node->end(); I!=E; ++I){
      Function* F = I->second->getFunction();
      if(!F || F->empty()) continue; // this is a external function
      lastC = I;
   }
   for(auto I = node->begin(), E = node->end(); I!=E; ++I){
      Function* F = I->second->getFunction();
      if(!F || F->empty()) continue; // this is a external function
      bool is_last = I == lastC;

      for(auto b = level.begin();b!=level.end()-1;++b) errs()<<(*b?ancient:empty);
      errs()<<(is_last?last:parent)<<" "<<F->getName()<<"\n";
      if(is_last) level.back()=0;
      print_cg(I->second);
   }
   level.pop_back();
}
#endif

bool PrintLibCall::runOnFunction(Function &F)
{
   int empty = 1;
   for(auto I = inst_begin(F), E = inst_end(F); I!=E; ++I){
      if(CallInst* CI = dyn_cast<CallInst>(&*I)){
         Function* Func = dyn_cast<Function>(castoff(CI->getCalledValue()));
         if(!Func) continue;
         auto FuncInfo = LC.getFunctionInfo(Func);
         if(FuncInfo && FuncInfo->UniversalBehavior &
               AliasAnalysis::ModRefResult::Mod){ /* the function writes
                                                     memory */
            if(empty){
               errs()<<F.getName()<<":\n";
               empty = 0;
            }

            errs()<<"   "<<Func->getName()<<"\n";
         }
         
      }
   }
   if(!empty) errs()<<"\n";
   return false;
}
