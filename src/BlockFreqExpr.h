#ifndef BLOCKFREQEXPR_H_H
#define BLOCKFREQEXPR_H_H
#include <llvm/Pass.h>
#include <llvm/Support/BlockFrequency.h>
#include <llvm/Support/BranchProbability.h>

namespace lle {
   /** a simple wrap for BlockFrequencyInfo
    * if a BasicBlock in a Loop, it returns a pair <Prob,TripCount>
    * which Prob = \frac {bfreq_{LLVM}(BB)} {bfreq_{LLVM}(Header)}
    * according to Mathematica Notebook
    * TripCount is come from LoopCycle Pass.
    * This block's frequency equals to Prob \times TripCount
    *
    * if a BasicBlock doesn't in a Loop, it returns normal BlockFrequency
    */
   class BlockFreqExpr: public llvm::FunctionPass
   {
      public:
      static char ID;
      BlockFreqExpr();
      void getAnalysisUsage(llvm::AnalysisUsage&) const override;
      bool runOnFunction(llvm::Function& F) override;
      /** try to get <Prob,TripCount>. if failed. it returns <0,nullptr> */
      std::pair<llvm::BranchProbability, llvm::Value*> 
         getBlockFreqExpr(llvm::BasicBlock* BB) const;
      /** if getBlockFreqExpr failed, use this to get normal BlockFrequency */
      llvm::BlockFrequency getBlockFreq(llvm::BasicBlock* BB) const;
   };

   llvm::BranchProbability operator/(
         const llvm::BlockFrequency& LHS,
         const llvm::BlockFrequency& RHS
         );
   //scale prob with a small number using gcd.
   llvm::BranchProbability scale(const llvm::BranchProbability& prob);
}
#endif
