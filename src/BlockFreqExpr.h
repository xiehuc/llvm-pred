#ifndef BLOCKFREQEXPR_H_H
#define BLOCKFREQEXPR_H_H
#include "loop.h"
#include <llvm/Pass.h>
#include <llvm/Support/BlockFrequency.h>
#include <llvm/Support/BranchProbability.h>

namespace lle {
   class BlockFreqExpr: public llvm::FunctionPass
   {
      LoopCycle LC;
      public:
      static char ID;
      BlockFreqExpr();
      void getAnalysisUsage(llvm::AnalysisUsage&) const override;
      bool runOnFunction(llvm::Function& F) override;
      std::pair<llvm::BranchProbability, llvm::Value*> 
         getBlockFreqExpr(llvm::BasicBlock* BB) const;
      llvm::BlockFrequency getBlockFreq(llvm::BasicBlock* BB) const;
   };

   llvm::BranchProbability operator/(
         const llvm::BlockFrequency& LHS,
         const llvm::BlockFrequency& RHS
         );
}
#endif
