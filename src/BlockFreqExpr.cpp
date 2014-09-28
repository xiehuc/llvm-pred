#include "preheader.h"
#include <llvm/Analysis/BlockFrequencyInfo.h>
#include <llvm/Support/BranchProbability.h>
#include <llvm/Analysis/LoopInfo.h>

#include "BlockFreqExpr.h"
#include "loop.h"
#include "debug.h"

using namespace lle;
using namespace llvm;

char BlockFreqExpr::ID = 0;

static RegisterPass<BlockFreqExpr> X("Block-freq-expr", "Block Frequency Expression Analysis", true, true);

BlockFreqExpr::BlockFreqExpr() : FunctionPass(ID) { }

void BlockFreqExpr::getAnalysisUsage(AnalysisUsage &AU) const
{
   AU.addRequired<BlockFrequencyInfo>();
   AU.addRequired<LoopInfo>();
}

bool BlockFreqExpr::runOnFunction(Function &F) 
{
   LoopInfo& LI = getAnalysis<LoopInfo>();
   BlockFrequencyInfo& BFI = getAnalysis<BlockFrequencyInfo>();
   LoopCycle LC(this);

   for(Loop* L : LI){
      errs()<<LC.getOrInsertCycle(L)<<"\n";
      BlockFrequency HeaderF = BFI.getBlockFreq(L->getHeader());
      for(auto BB =L->block_begin(), BE = L->block_end(); BB!=BE; ++BB){
         errs()<<(BFI.getBlockFreq(*BB)/HeaderF)<<"\n";
      }
   }
   return false;
}

BlockFrequency BlockFreqExpr::getBlockFreq(BasicBlock* BB)
{
   return getAnalysis<BlockFrequencyInfo>().getBlockFreq(BB);
}

Value* BlockFreqExpr::getBlockFreqExpr(BasicBlock *BB)
{
   return NULL;
}


BranchProbability lle::operator/(
      const BlockFrequency& LHS, 
      const BlockFrequency& RHS)
{
   return BranchProbability(LHS.getFrequency(), RHS.getFrequency());
}
