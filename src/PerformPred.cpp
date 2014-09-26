#include "preheader.h"
#include <llvm/Analysis/BranchProbabilityInfo.h>
#include <llvm/Analysis/BlockFrequencyInfo.h>
#include <llvm/Analysis/CallGraphSCCPass.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/CFG.h>

namespace lle {
   class PerformPred;
};

class lle::PerformPred : public llvm::CallGraphSCCPass
{
   public:
   static char ID;
   PerformPred():llvm::CallGraphSCCPass(ID) {}
   void calc(llvm::Function* F);
   bool runOnSCC(llvm::CallGraphSCC& SCC) override;
   void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
};

using namespace llvm;
using namespace lle;

char PerformPred::ID = 0;
static RegisterPass<PerformPred> X("PerfPred", "get Performance Predication Model");

void PerformPred::getAnalysisUsage(AnalysisUsage &AU) const
{
   CallGraphSCCPass::getAnalysisUsage(AU);
   AU.addRequired<BranchProbabilityInfo>();
   AU.addRequired<BlockFrequencyInfo>();
}

bool PerformPred::runOnSCC(CallGraphSCC &SCC)
{
   for(auto CG : SCC){
      Function* F = CG->getFunction();
      if(F && !F->isDeclaration()){
         calc(F);
      }
   }
   return false;
}

void PerformPred::calc(Function *F)
{
   BasicBlock* Entry = F->getEntryBlock();
   countOn(Entry);
   for(succ_begin(Entry)
   
}
