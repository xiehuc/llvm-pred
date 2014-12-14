#include "preheader.h"
#include "Adaptive.h"
#define private public
#include "../llvm/DeadArgumentElimination.cpp"

using namespace lle;
using namespace llvm;

DAE_Adaptive::DAE_Adaptive(ModulePass* DAE)
{
   opaque = DAE;
}

void DAE_Adaptive::prepare(Module* M)
{
   ::DAE* dae = static_cast<::DAE*>(opaque);
   for (Module::iterator I = M->begin(), E = M->end(); I != E; ) {
      Function &F = *I++;
      if (F.getFunctionType()->isVarArg())
         dae->DeleteDeadVarargs(F);
   }
   for (Module::iterator I = M->begin(), E = M->end(); I != E; ++I)
      dae->SurveyFunction(*I);
}

void DAE_Adaptive::runOnFunction(Function &F)
{
   ::DAE* dae = static_cast<::DAE*>(opaque);
   dae->RemoveDeadStuffFromFunction(&F);
   dae->RemoveDeadArgumentsFromCallers(F);
}
