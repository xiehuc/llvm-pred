/** provide Global Variable Info Lookup */
#ifndef GVINFO_H_H
#define GVINFO_H_H
#include <llvm/Pass.h>
#include <llvm/IR/Constant.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/Instructions.h>
#include <list>

namespace lle{
   class GVInfo;
};

class lle::GVInfo: public llvm::ModulePass
{
   struct Data {
      llvm::StoreInst* store;
      std::list<llvm::Instruction*> resolve;
   };
   llvm::DenseMap<llvm::Constant*, Data> Info;

   bool findStoreOnGV(llvm::Value*, llvm::Constant*);
   public:
      static char ID;
      explicit GVInfo():ModulePass(ID) {}
      bool runOnModule(llvm::Module& M) override;
      void getAnalysisUsage(llvm::AnalysisUsage&) const override;

      /*
      llvm::ConstantExpr* lookup(llvm::Value*);
      */
      llvm::Value* getKey(llvm::Constant*);
      llvm::Value* getValue(llvm::Constant*);
      void print(llvm::raw_ostream&, const llvm::Module*) const override;
};
#endif
