#include "preheader.h"
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/ValueSymbolTable.h>
#include <unordered_map>
#include <random>
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
	  llvm::Value* implyLoadTemplate(llvm::CallInst* Template, llvm::Value* var) const;
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

   if((F = M.getFunction("inst_template")))
   {
	   for(auto Ite = user_begin(F), E = user_end(F); Ite!=E; ++Ite){
		  CallInst* Template = dyn_cast<CallInst>(*Ite);
		  Value* R = implyTemplate(Template);
		  Template->replaceAllUsesWith(R);
		  Template->eraseFromParent();
	   }
   }
   else if((F = M.getFunction("load_template")))
   {
        Function* mainf = M.getFunction("main");
        Assert(mainf,"can not find main");
        ValueSymbolTable& symboltable = mainf->getValueSymbolTable();
        StringRef str("loadvar");
        Value* var = symboltable.lookup(str);
        for(auto Ite = user_begin(F), E = user_end(F); Ite!=E; ++Ite)
        {
           CallInst* Template = dyn_cast<CallInst>(*Ite);
           Value* R = implyLoadTemplate(Template,var);
           Template->replaceAllUsesWith(R);
           Template->eraseFromParent();
        }
   }
   Assert(F, "can not find load_template");  
   return true;
}

static Value* load(Instruction *InstPoint, Value* var)
{
	for(int i=0;i<10000;++i)
	{
		LoadInst* newload = new LoadInst(var,"",InstPoint);
        newload->setVolatile(true);
	}	
	return var;
}
Value* lle::InstTiming::implyLoadTemplate(CallInst *Template, Value *var) const
{
   Assert(Template,"Template is empty");
   ConstantExpr* Arg0 = dyn_cast<ConstantExpr>(Template->getArgOperand(0));
   Assert(Arg0,"");
   GlobalVariable* Global = dyn_cast<GlobalVariable>(Arg0->getOperand(0));
   Assert(Global,"");
   ConstantDataArray* Const = dyn_cast<ConstantDataArray>(Global->getInitializer());
   Assert(Const,"");
   //StringRef Selector = Const->getAsCString();

   //auto Found = ImplMap.find(Selector.str());
   //AssertRuntime(Found != ImplMap.end(), "unknow template keyword: "<<Selector);

   return load(Template,var);
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

#define REPEAT 10000

static Value* fix_add(Instruction *InsPoint)
{
   //Lock& L = getAnalysis<Lock>();

   Type* I32Ty = Type::getInt32Ty(InsPoint->getContext());
   Value* One = ConstantInt::get(I32Ty, 1);
   Value* Lhs = One;
   for(int i=0;i<REPEAT;++i){
      Lhs = BinaryOperator::CreateAdd(Lhs, One, "", InsPoint);
      //Lhs = L.lock_inst(cast<Instruction>(Lhs));
   }
   return Lhs;
}

static Value* float_add(Instruction* InsPoint)
{
   Type* FTy = Type::getDoubleTy(InsPoint->getContext());
   Value* One = ConstantFP::get(FTy, 1.0L);
   Value* Lhs = One;
   for(int i=0;i<REPEAT;++i){
      Lhs = BinaryOperator::CreateFAdd(Lhs, One, "", InsPoint);
      //Lhs = L.lock_inst(cast<Instruction>(Lhs));
   }
   return CastInst::Create(CastInst::FPToSI, Lhs,
         Type::getInt32Ty(InsPoint->getContext()), "", InsPoint);
}

static Value* mix_add(Instruction* InsPoint)
{
   Type* FTy = Type::getDoubleTy(InsPoint->getContext());
   Type* I32Ty = Type::getInt32Ty(InsPoint->getContext());
   Value* F_One = ConstantFP::get(FTy, 1.0L);
   Value* I_One = ConstantInt::get(I32Ty, 1);
   Value* F_Lhs = F_One, *I_Lhs = I_One;
   for(int i=0;i<REPEAT;++i){
      I_Lhs = BinaryOperator::CreateAdd(I_Lhs, I_One, "", InsPoint);
      F_Lhs = BinaryOperator::CreateFAdd(F_Lhs, F_One, "", InsPoint);
   }
   Value* I_Rhs = CastInst::Create(CastInst::FPToSI, F_Lhs, I32Ty, "", InsPoint);
   return BinaryOperator::CreateAdd(I_Lhs, I_Rhs, "", InsPoint);
}

static Value* rand_add(Instruction* InsPoint)
{
   Type* FTy = Type::getDoubleTy(InsPoint->getContext());
   Type* I32Ty = Type::getInt32Ty(InsPoint->getContext());
   Value* F_One = ConstantFP::get(FTy, 1.0L);
   Value* I_One = ConstantInt::get(I32Ty, 1);
   Value* F_Lhs = F_One, *I_Lhs = I_One;
   std::random_device rd;
   std::mt19937 gen(rd());
   std::bernoulli_distribution d(0.5);
   for(int i=0;i<REPEAT;++i){
      if(d(gen))
         I_Lhs = BinaryOperator::CreateAdd(I_Lhs, I_One, "", InsPoint);
      else
         F_Lhs = BinaryOperator::CreateFAdd(F_Lhs, F_One, "", InsPoint);
   }
   Value* I_Rhs = CastInst::Create(CastInst::FPToSI, F_Lhs, I32Ty, "", InsPoint);
   return BinaryOperator::CreateAdd(I_Lhs, I_Rhs, "", InsPoint);
}

std::unordered_map<std::string, lle::InstTiming::TemplateFunc> 
lle::InstTiming::ImplMap = {
   {"fix_add",   fix_add}, 
   {"float_add", float_add},
   {"mix_add",   mix_add},
   {"rand_add",  rand_add}
};
