#include "preheader.h"
#include <llvm/Analysis/BlockFrequencyInfo.h>
#include <llvm/Support/BranchProbability.h>
#include <llvm/Analysis/LoopInfo.h>

#include "BlockFreqExpr.h"
#include "debug.h"

using namespace lle;
using namespace llvm;

char BlockFreqExpr::ID = 0;

static RegisterPass<BlockFreqExpr> X("Block-freq-expr", "Block Frequency Expression Analysis", true, true);

BlockFreqExpr::BlockFreqExpr() : FunctionPass(ID), LC(this) { }

void BlockFreqExpr::getAnalysisUsage(AnalysisUsage &AU) const
{
   AU.addRequired<BlockFrequencyInfo>();
   AU.addRequired<LoopInfo>();
}

bool BlockFreqExpr::runOnFunction(Function &F) 
{
   LoopInfo& LI = getAnalysis<LoopInfo>();

   for(Loop* L : LI){
      LC.getOrInsertCycle(L);
   }
   return true;
}

BlockFrequency BlockFreqExpr::getBlockFreq(BasicBlock* BB) const
{
   return getAnalysis<BlockFrequencyInfo>().getBlockFreq(BB);
}

std::pair<BranchProbability, Value*> BlockFreqExpr::getBlockFreqExpr(BasicBlock *BB) const
{
   LoopInfo& LI = getAnalysis<LoopInfo>();
   BlockFrequencyInfo& BFI = getAnalysis<BlockFrequencyInfo>();
   Loop* L = LI.getLoopFor(BB);
   if(L==NULL) return std::make_pair(BranchProbability(0, 0), nullptr);

   BlockFrequency HeaderF = BFI.getBlockFreq(L->getHeader());
   return std::make_pair(BFI.getBlockFreq(BB)/HeaderF, LC.getLoopCycle(L));
}


BranchProbability lle::operator/(
      const BlockFrequency& LHS, 
      const BlockFrequency& RHS)
{
   return BranchProbability(LHS.getFrequency(), RHS.getFrequency());
}
