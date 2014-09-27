#ifndef LOOPSIMPLIFY_H_H
#define LOOPSIMPLIFY_H_H

#include <llvm/Analysis/LoopPass.h>

namespace lle{
   class LoopCycleSimplify;
};

class lle::LoopCycleSimplify:public llvm::LoopPass
{
   llvm::Loop* CurL;
   public:
   static char ID;
   explicit LoopCycleSimplify():LoopPass(ID){}
   void getAnalysisUsage(llvm::AnalysisUsage&) const override;
   bool runOnLoop(llvm::Loop* L,llvm::LPPassManager&) override;
   //bool runOnModule(llvm::Module&);
   void print(llvm::raw_ostream&,const llvm::Module*) const override;
};
#endif
