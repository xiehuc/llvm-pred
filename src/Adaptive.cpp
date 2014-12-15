#include "preheader.h"
#include "Adaptive.h"
#include <llvm/Pass.h>
#include "debug.h"

using namespace lle;
using namespace llvm;

Adaptive::Adaptive(FunctionPass* FP)
{
   opaque = FP;
}

void Adaptive::getAnalysisUsage(AnalysisUsage& AU) const
{
   FunctionPass* FP = static_cast<FunctionPass*>(opaque);
   FP->getAnalysisUsage(AU);
}

void Adaptive::prepare(Pass *P)
{
   FunctionPass* FP = static_cast<FunctionPass*>(opaque);
   FP->setResolver(P->getResolver());
}

void Adaptive::runOnFunction(Function& F)
{
   FunctionPass* FP = static_cast<FunctionPass*>(opaque);
   FP->runOnFunction(F);
}

