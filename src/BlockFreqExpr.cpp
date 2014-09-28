#include "preheader.h"
#include <llvm/Analysis/BlockFrequencyInfo.h>
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
   AU.addRequired<LPPassManager>();
   AU.addRequired<LoopInfo>();
}

bool BlockFreqExpr::runOnFunction(Function &F) 
{
   LoopInfo& LI = getAnalysis<LoopInfo>();
   /*LPPassManager& LPP = getAnalysis<LPPassManager>();
   LoopCycle LC;
   for(Loop* L : LI){
      LPP.insertLoopIntoQueue(L);
   }*/
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
