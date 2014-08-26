/** provide Global Variable Info Lookup */
#ifndef GVINFO_H_H
#define GVINFO_H_H
#include <llvm/Pass.h>
#include <llvm/IR/Constant.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>

namespace lle{
   class GVInfo;
};

class lle::GVInfo: public llvm::ModulePass
{
   struct Data {
      llvm::Instruction* store;
   };
   llvm::DenseMap<llvm::Value*, Data> Info;

   bool findStoreOnGV(llvm::Value*);
   public:
      static char ID;
      explicit GVInfo():ModulePass(ID) {}
      bool runOnModule(llvm::Module& M) override;
      void getAnalysisUsage(llvm::AnalysisUsage&) const override;

      /*
      llvm::ConstantExpr* lookup(llvm::Value*);
      llvm::Instruction* getKey(llvm::ConstantExpr*);
      llvm::Instruction* getValue(llvm::ConstantExpr*);
      */
};
#endif
