#include "preheader.h"
#include "Adaptive.h"
#include "../llvm/DeadStoreElimination.cpp"

using namespace lle;
using namespace llvm;

DSE_Adaptive::DSE_Adaptive(FunctionPass* DSE)
{
   opaque = DSE;
}

void DSE_Adaptive::getAnalysisUsage(AnalysisUsage& AU) const
{
   ::DSE* dse_ = static_cast<::DSE*>(opaque);
   dse_->getAnalysisUsage(AU);
}

void DSE_Adaptive::prepare(Pass* P)
{
   AA = &P->getAnalysis<AliasAnalysis>();
   TLI = AA->getTargetLibraryInfo();
}

void DSE_Adaptive::prepare(Function* F, Pass* P)
{
   ::DSE* dse_ = static_cast<::DSE*>(opaque);
   MD = &P->getAnalysis<MemoryDependenceAnalysis>(*F);
   DT = &P->getAnalysis<DominatorTreeWrapperPass>(*F).getDomTree();
   dse_->MD = MD;
   dse_->AA = AA;
   dse_->TLI = TLI;
   dse_->DT = DT;
}

void DSE_Adaptive::runOnBasicBlock(BasicBlock& BB)
{
   ::DSE* dse_ = static_cast<::DSE*>(opaque);
   dse_->runOnBasicBlock(BB);
}

void DSE_Adaptive::DeleteDeadInstruction(llvm::Instruction* I)
{
   ::DeleteDeadInstruction(I, *MD, TLI);
}
