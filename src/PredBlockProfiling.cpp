#include "preheader.h"
#include "PredBlockProfiling.h"
#include "ProfilingUtils.h"

#include <llvm/IR/Module.h>
#include <llvm/Pass.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;
using namespace std;

char PredBlockProfiler::ID = 0;
PredBlockProfiler* PredBlockProfiler::ins = NULL;

static RegisterPass<PredBlockProfiler> X("insert-pred-profiling", "Insert Block Predicate Profiling into Module", false, true);

PredBlockProfiler::PredBlockProfiler():ModulePass(ID)
{
   PredBlockProfiler::ins = this;
}


static void IncrementBlockCounters(llvm::Value* Inc, unsigned Index, GlobalVariable* Counters, IRBuilder<>& Builder)
{
   LLVMContext &Context = Inc->getContext();

   // Create the getelementptr constant expression
   std::vector<Constant*> Indices(2);
   Indices[0] = Constant::getNullValue(Type::getInt32Ty(Context));
   Indices[1] = ConstantInt::get(Type::getInt32Ty(Context), Index);
   Constant *ElementPtr =
      ConstantExpr::getGetElementPtr(Counters, Indices);

   // Load, increment and store the value back.
   Value* OldVal = Builder.CreateLoad(ElementPtr, "OldBlockCounter");
   Value* NewVal = Builder.CreateAdd(OldVal, Inc, "NewBlockCounter");
   Builder.CreateStore(NewVal, ElementPtr);
}

bool PredBlockProfiler::runOnModule(Module& M)
{
   unsigned Idx;
   IRBuilder<> Builder(M.getContext());

   unsigned NumBlocks = 0;
   for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) 
      NumBlocks += F->size();

	Type*ATy = ArrayType::get(Type::getInt32Ty(M.getContext()),NumBlocks);
	GlobalVariable* Counters = new GlobalVariable(M, ATy, false,
			GlobalVariable::InternalLinkage, Constant::getNullValue(ATy),
			"BlockPredCounters");

   for(auto F = M.begin(), FE = M.end(); F != FE; ++F){
      for(auto BB = F->begin(), BBE = F->end(); BB != BBE; ++BB){
         auto Found = BlockTraps.find(BB);
         if(Found == BlockTraps.end()){
            Idx++;
         }else{
            Value* Inc = Found->second.first;
            Value* InsertPos = Found->second.second;
            if(BasicBlock* InsertBB = dyn_cast<BasicBlock>(InsertPos))
               Builder.SetInsertPoint(InsertBB);
            else if(Instruction* InsertI = dyn_cast<Instruction>(InsertPos))
               Builder.SetInsertPoint(InsertI);
            else assert(0 && "unknow insert position type");
            IncrementBlockCounters(Inc, Idx++, Counters, Builder);
         }
      }
   }

	Function* Main = M.getFunction("main");
	InsertProfilingInitCall(Main, "llvm_start_pred_block_profiling", Counters);
	return true;
}
