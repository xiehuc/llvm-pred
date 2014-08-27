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
      bool write_once = true;
      llvm::StoreInst* store; // last store inst, invalid when !write_once
      std::list<const llvm::Instruction*> resolve; // resolve link
   };
   struct ArgInfo {
      bool write_once;
      llvm::GlobalVariable* param; 
      // valid when write_once || there are many callinst, and every callinst's
      // param is the same Global Variable
   };
   llvm::DenseMap<llvm::Constant*, Data> info;
   llvm::DenseMap<llvm::Argument*, ArgInfo> arg_info;

   bool findLoadOnGVPointer(llvm::Value*, llvm::Constant*);
   bool findStoreOnGV(llvm::Value*, llvm::Constant*);
   public:
      static char ID;
      explicit GVInfo():ModulePass(ID) {}
      bool runOnModule(llvm::Module& M) override;
      void getAnalysisUsage(llvm::AnalysisUsage&) const override;

      /* if Value is a GetElementPtr Instruction, tring covert it to a
       * GetElementPtr ConstantExpr , using ArgInfo*/
      llvm::Constant* lookup(llvm::Value*) const;
      // return store pointer on GlobalVariable, valid only write_once
      const llvm::Value* getKey(llvm::Constant* C) const {
         auto found = info.find(C);
         if(found == info.end()) return NULL;
         auto SI = found->second.store;
         return (SI && found->second.write_once)?
            SI->getPointerOperand(): NULL;
      }
      // return store value on GlobalVariable, valid only write_once
      const llvm::Value* getValue(llvm::Constant* C) const {
         auto found = info.find(C);
         if(found == info.end()) return NULL;
         auto SI = found->second.store;
         return (SI && found->second.write_once)?
            SI->getValueOperand(): NULL;
      }
      const std::list<const llvm::Instruction*>* getResolve(llvm::Constant* C) const {
         auto found = info.find(C);
         if(found == info.end()) return NULL;
         return &found->second.resolve;
      }
      void print(llvm::raw_ostream&, const llvm::Module*) const override;
};
#endif
