#include <llvm/Pass.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/Support/raw_ostream.h>

#include <string>

namespace lle{
   class PrintEnv;
   class PrintCgTree;
   /* class DotCgTree;*/
}

using namespace std;
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

class lle::PrintCgTree: public ModulePass
{
   void print_cg(CallGraphNode* node);
   public:
   static char ID;
   PrintCgTree():ModulePass(ID) {}
   bool runOnModule(Module& M);
};

char PrintEnv::ID = 0;
char PrintCgTree::ID = 0;

static RegisterPass<PrintEnv> X("Env","print environment params", true, true);
static RegisterPass<PrintCgTree> Y("Cg", "print Callgraph Tree", true, true);

bool PrintEnv::runOnModule(Module& M)
{
#define printenv(env) errs()<<env<<":   "<<(getenv(env)?:"")<<"\n";
   printenv("SHRINK_LEVEL");
   printenv("LIBCALL_FILE");
   printenv("IGNOREFUNC_FILE");
   return false;
}

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

