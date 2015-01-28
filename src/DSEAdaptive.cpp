#include "preheader.h"
#include "../llvm/DeadStoreElimination.cpp"
#include "Adaptive.h"
#include "Resolver.h"
#include <llvm/ADT/DepthFirstIterator.h>
#include "ddg.h"

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

void DSE_Adaptive::DeleteCascadeInstruction(llvm::Instruction* I)
{
   lle::ResolveEngine RE;
   RE.addRule(RE.ibase_rule);
   auto ddg = RE.resolve(I);
   if(ddg.empty()){
      DeleteDeadInstruction(I);
      return;
   }
   // FIXME there should use ipo_begin, ipo_end
   // because inversed search for ResolveEngine always return a single link
   // so, it can also use df_begin, df_end
   for(auto Ite = df_begin(&ddg), E = df_end(&ddg); Ite!=E; ++Ite){
      Instruction* II = dyn_cast<Instruction>(DataDepGraph::get_user(**Ite));
      II->replaceAllUsesWith(UndefValue::get(II->getType()));
      ::DeleteDeadInstruction(II, *MD, TLI);
   }
}
