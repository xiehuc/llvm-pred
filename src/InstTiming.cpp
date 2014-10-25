#include "preheader.h"
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>

//#include "LockInst.h"
#include "util.h"
#include "debug.h"

namespace lle{
   class InstTiming: public llvm::ModulePass
   {
      public:
      static char ID;
      InstTiming():ModulePass(ID) {}
      //void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
      bool runOnModule(llvm::Module& M) override;
      private:
      llvm::Value* insertAdd(llvm::Instruction* InsPoint);
   };
};

using namespace llvm;
char lle::InstTiming::ID = 0;
static RegisterPass<lle::InstTiming> X("InstTiming", "InstTiming");

#if 0
void lle::InstTiming::getAnalysisUsage(AnalysisUsage &AU) const
{
   AU.addRequired<Lock>();
}
#endif

bool lle::InstTiming::runOnModule(Module &M)
{
   Function* F = M.getFunction("timing");
   Assert(F, "unable find @timing function");
   F->addFnAttr(Attribute::AlwaysInline);

   F = M.getFunction("timing_res");
   Assert(F, "unable find @timing_res function");
   F->addFnAttr(Attribute::AlwaysInline);

   F = M.getFunction("inst_template");
   Assert(F, "unable find @inst_template function");
   for(auto Ite = user_begin(F), E = user_end(F); Ite!=E; ++Ite){
      Instruction* I = dyn_cast<Instruction>(*Ite);
      Value* R = insertAdd(I);
      I->replaceAllUsesWith(R);
      I->eraseFromParent();
   }
   return true;
}

Value* lle::InstTiming::insertAdd(Instruction *InsPoint)
{
   //Lock& L = getAnalysis<Lock>();

   Type* I32Ty = Type::getInt32Ty(InsPoint->getContext());
   Value* One = ConstantInt::get(I32Ty, 1);
   Value* Lhs = One;
   for(int i=0;i<1;++i){
      Lhs = BinaryOperator::CreateAdd(Lhs, One, "", InsPoint);
      //Lhs = L.lock_inst(cast<Instruction>(Lhs));
   }
   return Lhs;
}
