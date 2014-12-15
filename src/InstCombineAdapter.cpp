#include "preheader.h"
#include "Adaptive.h"
#include <llvm/Pass.h>
#include "../llvm/InstCombine.h"
#include "debug.h"

using namespace lle;
using namespace llvm;

InstCombine_Adaptive::InstCombine_Adaptive(FunctionPass* FP)
{
   opaque = FP;
}
const void* InstCombine_Adaptive::id() const
{
   FunctionPass* FP = static_cast<FunctionPass*>(opaque);
   return FP->getPassID();
}

void InstCombine_Adaptive::getAnalysisUsage(AnalysisUsage& AU) const
{
   FunctionPass* FP = static_cast<FunctionPass*>(opaque);
   FP->getAnalysisUsage(AU);
}

void InstCombine_Adaptive::prepare(Pass *P)
{
   llvm::InstCombiner* ic = static_cast<llvm::InstCombiner*>(opaque);
   ic->DL = nullptr;
}

void InstCombine_Adaptive::runOnFunction(Function& F)
{
   FunctionPass* FP = static_cast<FunctionPass*>(opaque);
   FP->runOnFunction(F);
}

