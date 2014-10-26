#include "preheader.h"
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>

#include <unordered_map>
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
      typedef llvm::Value* (*TemplateFunc)(llvm::Instruction* InsPoint);
      static std::unordered_map<std::string, TemplateFunc> ImplMap;
      llvm::Value* implyTemplate(llvm::CallInst* Template) const;
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
      CallInst* Template = dyn_cast<CallInst>(*Ite);
      Value* R = implyTemplate(Template);
      Template->replaceAllUsesWith(R);
      Template->eraseFromParent();
   }
   return true;
}

Value* lle::InstTiming::implyTemplate(CallInst *Template) const
{
   Assert(Template,"");
   ConstantExpr* Arg0 = dyn_cast<ConstantExpr>(Template->getArgOperand(0));
   Assert(Arg0,"");
   GlobalVariable* Global = dyn_cast<GlobalVariable>(Arg0->getOperand(0));
   Assert(Global,"");
   ConstantDataArray* Const = dyn_cast<ConstantDataArray>(Global->getInitializer());
   Assert(Const,"");
   StringRef Selector = Const->getAsCString();

   auto Found = ImplMap.find(Selector.str());
   AssertRuntime(Found != ImplMap.end(), "unknow template keyword: "<<Selector);

   return Found->second(Template);
}

static Value* fix_add(Instruction *InsPoint)
{
   //Lock& L = getAnalysis<Lock>();

   Type* I32Ty = Type::getInt32Ty(InsPoint->getContext());
   Value* One = ConstantInt::get(I32Ty, 1);
   Value* Lhs = One;
   for(int i=0;i<100;++i){
      Lhs = BinaryOperator::CreateAdd(Lhs, One, "", InsPoint);
      //Lhs = L.lock_inst(cast<Instruction>(Lhs));
   }
   return Lhs;
}

std::unordered_map<std::string, lle::InstTiming::TemplateFunc> 
lle::InstTiming::ImplMap = {
   {"fix_add", fix_add},
};
