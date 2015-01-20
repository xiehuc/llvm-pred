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
#include <iostream>
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

static std::string FunctyStr = "";


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
	   for(auto Ite = F->user_begin(), E = F->user_end(); Ite!=E; ++Ite){
		  CallInst* Template = dyn_cast<CallInst>(*Ite);
		  Value* R = implyTemplate(Template);
		  Template->replaceAllUsesWith(R);
		  Template->eraseFromParent();
	   }
   }
   else 
      Assert(F, "can not find @inst_template");  
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

   FunctyStr = Selector.str();
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
   Value* One = ConstantFP::get(FTy, 1.003L);
   Value* Lhs = One;
   for(int i=0;i<REPEAT;++i){
      Lhs = BinaryOperator::Create(Instruction::FAdd, Lhs, One, "", InsPoint);
      //Lhs = L.lock_inst(cast<Instruction>(Lhs));
   }
   return CastInst::Create(CastInst::FPToSI, Lhs,
         Type::getInt32Ty(InsPoint->getContext()), "", InsPoint);
}

static Value* fix_sub(Instruction* InsPoint){
   Type* I32Ty = Type::getInt32Ty(InsPoint->getContext());
   Value* One = ConstantInt::get(I32Ty, 10);
   Value* Lhs = ConstantInt::get(I32Ty, 20*REPEAT);
   for(int i = 0;i < REPEAT; ++i){
      Lhs = BinaryOperator::Create(Instruction::Sub, Lhs, One,"",InsPoint);
   }
   return Lhs;
}
static Value* float_sub(Instruction* InsPoint){
   Type* FTy = Type::getDoubleTy(InsPoint->getContext());
   Value* One = ConstantFP::get(FTy,10.1L);
   Value* Lhs = ConstantFP::get(FTy,20L*REPEAT);
   for(int i = 0;i < REPEAT;++i){
      Lhs = BinaryOperator::Create(Instruction::FSub,Lhs,One,"",InsPoint);
   }
   return CastInst::Create(CastInst::FPToSI,Lhs,Type::getInt32Ty(InsPoint->getContext()),"",InsPoint);
}

static Value* fix_mul(Instruction* InsPoint){

   Type* I32Ty = Type::getInt32Ty(InsPoint->getContext());
   Value* One = ConstantInt::get(I32Ty,11);
   Value* Lhs = One;
   for(int i = 0;i < REPEAT;++i){
      Lhs = BinaryOperator::Create(Instruction::Mul,Lhs,One,"",InsPoint);
   }
   return Lhs;
}
static Value* float_mul(Instruction* InsPoint){
   Type* FTy = Type::getDoubleTy(InsPoint->getContext());
   Value* One = ConstantFP::get(FTy,1.03L);
   Value* Lhs = One;
   for(int i = 0;i < REPEAT;++i){
      One = ConstantFP::get(FTy,(double)(i+0.03));
      Lhs = BinaryOperator::Create(Instruction::FMul,Lhs,One,"",InsPoint);
   }
   return CastInst::Create(CastInst::FPToSI,Lhs,Type::getInt32Ty(InsPoint->getContext()),"",InsPoint);
}
static Value* us_div_rem(Instruction* InsPoint){
   Type* I32Ty = Type::getInt32Ty(InsPoint->getContext());
   Value* One = ConstantInt::get(I32Ty,10);
   Value* Lhs = One;
   for(int i = 0;i < REPEAT;++i){
      One = ConstantInt::get(I32Ty,i+3);
      if(FunctyStr == "u_div"){
         Lhs = BinaryOperator::Create(Instruction::UDiv,Lhs,One,"",InsPoint);
      }
      else if(FunctyStr == "s_div"){
         Lhs = BinaryOperator::Create(Instruction::SDiv,Lhs,One,"",InsPoint);
      }
      else if(FunctyStr == "u_rem"){
         Lhs = BinaryOperator::Create(Instruction::URem,Lhs,One,"",InsPoint);
      }
      else if(FunctyStr == "s_rem"){
         Lhs = BinaryOperator::Create(Instruction::SRem,Lhs,One,"",InsPoint);
      }
   }
   return Lhs;
}
static Value* float_div_rem(Instruction* InsPoint){
   Type* FTy = Type::getDoubleTy(InsPoint->getContext());
   Value* One = ConstantFP::get(FTy,(double)10.03);
   Value* Lhs = One;
   for(int i = 0;i < REPEAT; ++i){
      One = ConstantFP::get(FTy, (double)(i+1.03));
      if(FunctyStr == "float_div")
         Lhs = BinaryOperator::Create(Instruction::FDiv,One,Lhs,"",InsPoint);
      else if(FunctyStr == "float_rem")
         Lhs = BinaryOperator::Create(Instruction::FRem,One,Lhs,"",InsPoint);

   }
   return CastInst::Create(CastInst::FPToSI,Lhs,Type::getInt32Ty(InsPoint->getContext()),"",InsPoint);
}
static Value* binary_op(Instruction* InsPoint){
   Type* I32Ty = Type::getInt32Ty(InsPoint->getContext());
   Value* One = ConstantInt::get(I32Ty,3);
   Value* Lhs = One;
   for(int i = 0;i < REPEAT; ++i){
      if(FunctyStr == "shl"){
         Lhs = BinaryOperator::Create(Instruction::Shl,Lhs,One,"",InsPoint);
      }
      else if(FunctyStr == "lshr"){
         Lhs = BinaryOperator::Create(Instruction::LShr,Lhs,One,"",InsPoint);
      }
      else if(FunctyStr == "ashr"){
         Lhs = BinaryOperator::Create(Instruction::AShr,Lhs,One,"",InsPoint);
      }
      else if(FunctyStr == "and"){
         Lhs = BinaryOperator::Create(Instruction::And,Lhs,One,"",InsPoint);
      }
      else if(FunctyStr == "or"){
         Lhs = BinaryOperator::Create(Instruction::Or,Lhs,One,"",InsPoint);
      }
      else if(FunctyStr == "xor"){
         Lhs = BinaryOperator::Create(Instruction::Xor,Lhs,One,"",InsPoint);
      }
   }
   return Lhs;
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

static Value* load(Instruction* InsPoint)
{
   CallInst* Template = dyn_cast<CallInst>(InsPoint);
   Value* var = Template->getArgOperand(1);
   LoadInst* l;

	for(int i=0;i<REPEAT;++i)
	{
		l = new LoadInst(var,"",InsPoint);
      l->setVolatile(true);
	}	
	return l;
}
static Value* store(Instruction* InsPoint){
   CallInst* Template = dyn_cast<CallInst>(InsPoint);
   Value* var = Template->getArgOperand(1);
   StoreInst* s;

   for(int i = 0;i < REPEAT; ++i){
      Value* one = ConstantInt::get(InsPoint->getType(), i);
      s = new StoreInst(one, var,"",InsPoint);
      s->setVolatile(true);
   }
   return s;
}

std::unordered_map<std::string, lle::InstTiming::TemplateFunc> 
lle::InstTiming::ImplMap = {
   {"fix_add",   fix_add}, 
   {"float_add", float_add},
   {"mix_add",   mix_add},
   {"rand_add",  rand_add},
   {"load",      load},
   {"store", store},
   {"fix_sub",fix_sub},
   {"float_sub",float_sub},
   {"fix_mul",fix_mul},
   {"float_mul",float_mul},
   {"u_div",us_div_rem},
   {"s_div",us_div_rem},
   {"float_div",float_div_rem},
   {"u_rem",us_div_rem},
   {"s_rem",us_div_rem},
   {"float_rem",float_div_rem},
   {"shl",binary_op},
   {"lshr",binary_op},
   {"ashr",binary_op},
   {"and",binary_op},
   {"or",binary_op},
   {"xor",binary_op}
};
