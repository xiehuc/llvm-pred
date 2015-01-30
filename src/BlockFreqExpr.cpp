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
   LoopTripCount& LTC = getAnalysis<LoopTripCount>();
   LTC.updateCache(getAnalysis<LoopInfo>());
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
   return std::make_pair(BFI.getBlockFreq(BB)/HeaderF, LTC.getOrInsertTripCount(L));
}

bool BlockFreqExpr::inLoop(BasicBlock *BB) const
{
   return getAnalysis<LoopInfo>().getLoopFor(BB) != NULL;
}

// integer log2 of a float
static inline int32_t ilog2(float x)
{
	uint32_t ix = (uint32_t&)x;
	uint32_t exp = (ix >> 23) & 0xFF;
	int32_t log2 = int32_t(exp) - 127;

	return log2;
}
// http://stereopsis.com/log2.html
// x should > 0
static inline int32_t ilog2(uint32_t x) {
	return ilog2((float)x);
}


BranchProbability lle::operator/(
      const BlockFrequency& LHS, 
      const BlockFrequency& RHS)
{
   uint64_t n = LHS.getFrequency(), d = RHS.getFrequency();
   if(n > UINT32_MAX || d > UINT32_MAX){
      unsigned bit = std::max(ilog2(uint32_t(n>>32)),ilog2(uint32_t(d>>32)))+1;
      n >>= bit;
      d >>= bit;
   }
   return scale(BranchProbability(n, d));
}

static uint32_t GCD(uint32_t A, uint32_t B) // 最大公约数
{
   uint32_t Max=A, Min=B;
   A<B?Max=B,Min=A:0;
   do{
      A=Min,B=Max%Min;
      A<B?Max=B,Min=A:Max=A,Min=B;
   }while(B!=0);
   return A;
}

BranchProbability lle::scale(const BranchProbability& prob)
{
   uint32_t n = prob.getNumerator(), d = prob.getDenominator();
   uint32_t gcd = GCD(n,d);
   return BranchProbability(n/gcd, d/gcd);
}
