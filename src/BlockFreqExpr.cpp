#include "preheader.h"
#include <llvm/Analysis/BlockFrequencyInfo.h>
#include <llvm/Support/BranchProbability.h>
#include <llvm/Analysis/LoopInfo.h>

#include "LoopTripCount.h"
#include "BlockFreqExpr.h"
#include "debug.h"

using namespace lle;
using namespace llvm;

char BlockFreqExpr::ID = 0;

static RegisterPass<BlockFreqExpr> X("Block-freq-expr", "Block Frequency Expression Analysis", true, true);

BlockFreqExpr::BlockFreqExpr() : FunctionPass(ID) { }

void BlockFreqExpr::getAnalysisUsage(AnalysisUsage &AU) const
{
   AU.addRequired<LoopTripCount>();
   AU.addRequired<BlockFrequencyInfo>();
   AU.addRequired<LoopInfo>();
}

bool BlockFreqExpr::runOnFunction(Function &F) 
{
   return false;
}

BlockFrequency BlockFreqExpr::getBlockFreq(BasicBlock* BB) const
{
   return getAnalysis<BlockFrequencyInfo>().getBlockFreq(BB);
}

std::pair<BranchProbability, Value*> BlockFreqExpr::getBlockFreqExpr(BasicBlock *BB) const
{
   LoopInfo& LI = getAnalysis<LoopInfo>();
   LoopTripCount& LTC = getAnalysis<LoopTripCount>();
   BlockFrequencyInfo& BFI = getAnalysis<BlockFrequencyInfo>();
   Loop* L = LI.getLoopFor(BB);
   if(L==NULL) return std::make_pair(BranchProbability(0, 0), nullptr);

   BlockFrequency HeaderF = BFI.getBlockFreq(L->getHeader());
   return std::make_pair(BFI.getBlockFreq(BB)/HeaderF, LTC.getTripCount(L));
}


BranchProbability lle::operator/(
      const BlockFrequency& LHS, 
      const BlockFrequency& RHS)
{
   return BranchProbability(LHS.getFrequency(), RHS.getFrequency());
}
