#ifndef BLOCKFREQEXPR_H_H
#define BLOCKFREQEXPR_H_H
#include <llvm/Pass.h>
#include <llvm/Support/BlockFrequency.h>

namespace lle {
   class BlockFreqExpr: public llvm::FunctionPass
   {
      public:
      static char ID;
      BlockFreqExpr();
      void getAnalysisUsage(llvm::AnalysisUsage&) const override;
      bool runOnFunction(llvm::Function& F) override;
      llvm::Value* getBlockFreqExpr(llvm::BasicBlock* BB);
      llvm::BlockFrequency getBlockFreq(llvm::BasicBlock* BB);
   };

   llvm::BranchProbability operator/(
         const llvm::BlockFrequency& LHS,
         const llvm::BlockFrequency& RHS
         );
}
#endif
