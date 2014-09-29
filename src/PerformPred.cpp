#include "preheader.h"
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/GraphWriter.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/CFG.h>
#include "BlockFreqExpr.h"
#include "Resolver.h"
#include "ddg.h"

#include <unordered_map>

namespace lle {
   class PerformPred;
};

class lle::PerformPred : public llvm::FunctionPass
{
   public:
   static char ID;
   PerformPred():llvm::FunctionPass(ID) {}
   bool runOnFunction(llvm::Function& F) override;
   //bool runOnSCC(llvm::CallGraphSCC& SCC) override;
   void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
   void print(llvm::raw_ostream&,const llvm::Module*) const override;

   private:
   llvm::GlobalVariable* cpu_times = NULL;
   llvm::Value* PrintSum; // a sum value used for print
   llvm::Value* cost(llvm::BasicBlock& BB, llvm::IRBuilder<>& Builder);
};

using namespace llvm;
using namespace lle;

char PerformPred::ID = 0;
static RegisterPass<PerformPred> X("PerfPred", "get Performance Predication Model");
enum InstTypes
{
   AddSubType, MulDivType, LSType, IgnoreType, NumTypes
};
static const char* const TypeNames[] = {"AddSub", "MulDiv", "Mem", "Save"};
static const std::unordered_map<int, InstTypes>  InstTMap = {
   {Instruction::Add,  AddSubType},
   {Instruction::Sub,  AddSubType},
   {Instruction::Mul,  MulDivType},
   {Instruction::UDiv, MulDivType},
   {Instruction::Load, LSType},
   {Instruction::Store,LSType}
};
static Value* Loads[NumTypes] = {NULL};

void PerformPred::getAnalysisUsage(AnalysisUsage &AU) const
{
   //CallGraphSCCPass::getAnalysisUsage(AU);
   AU.addRequired<BlockFreqExpr>();
   AU.addRequired<LoopInfo>();
}

/*
bool PerformPred::runOnSCC(CallGraphSCC &SCC)
{
   for(auto CG : SCC){
      Function* F = CG->getFunction();
      if(F && !F->isDeclaration()){
         calc(F);
      }
   }
   return false;
}
*/

bool PerformPred::runOnFunction(Function &F)
{
   if( F.isDeclaration() ) return false;
   BlockFreqExpr& BFE = getAnalysis<BlockFreqExpr>();
   LoopInfo& LI = getAnalysis<LoopInfo>();

   if(cpu_times == NULL){
      Type* ETy = Type::getInt32Ty(F.getContext());
      Type* ATy = ArrayType::get(ETy, NumTypes);
      cpu_times = new GlobalVariable(ATy, false, GlobalValue::InternalLinkage, ConstantFP::get(ETy, 1.0L), "cpuTime");
   }

   IRBuilder<> Builder(F.getEntryBlock().getTerminator());
   auto I32Ty = Type::getInt32Ty(F.getContext());
   Value* SumLhs = ConstantInt::get(I32Ty, 0), *SumRhs;
   for(auto& BB : F){
      SumRhs = cost(BB, Builder);
      auto FreqExpr = BFE.getBlockFreqExpr(&BB);
      if(FreqExpr.second == NULL){
         uint64_t freq = BFE.getBlockFreq(&BB).getFrequency();
         // XXX use scale in caculate
         SumRhs = Builder.CreateMul(SumRhs, ConstantInt::get(I32Ty, freq));
      }else{
         // XXX use scale in caculate
         uint64_t prob = FreqExpr.first.scale(1);
         auto InsertPos = LI.getLoopFor(&BB)->getLoopPreheader()->getTerminator();
         Builder.SetInsertPoint(InsertPos);
         Value* freq = Builder.CreateMul(FreqExpr.second, ConstantInt::get(I32Ty, prob));
         SumRhs = Builder.CreateMul(freq, SumRhs);
      }
      SumLhs = Builder.CreateAdd(SumLhs, SumRhs);
      SumLhs->setName(BB.getName()+".freq");
   }
   PrintSum = SumLhs;
   memset(Loads, 0, sizeof(Value*)*NumTypes);

   if(Ddg){
      lle::Resolver<NoResolve> R;
      auto Res = R.resolve(PrintSum);
      DDGraph ddg(Res, PrintSum);
      WriteGraph(&ddg, F.getName()+"-ddg");
   }

   return true;
}

void PerformPred::print(llvm::raw_ostream & OS, const llvm::Module * M) const
{
   lle::Resolver<NoResolve> R;
   auto Res = R.resolve(PrintSum);
   DDGraph ddg(Res, PrintSum);
   OS<<ddg.expr()<<"\n";
}

inline void count(llvm::BasicBlock &BB, unsigned int *counter)
{
   for(auto& I : BB){
      try{
         ++counter[InstTMap.at(I.getOpcode())];
      }catch(std::out_of_range e){
         //ignore exception
      }
   }
}

llvm::Value* PerformPred::cost(BasicBlock& BB, IRBuilder<>& Builder)
{
   unsigned InstCounts[NumTypes] = {0};

   count(BB, InstCounts);
   auto I32Ty = Type::getInt32Ty(BB.getContext());
   Value* Sum = ConstantInt::get(I32Ty, 0);

   for(unsigned Idx = 0; Idx<NumTypes; ++Idx){
      unsigned Num = InstCounts[Idx];
      if(Num == 0) continue;
      if(Loads[Idx]==NULL){
         Value* Arg[2] = {ConstantInt::get(I32Ty,0), ConstantInt::get(I32Ty, Idx)};
         Value* C = Builder.CreateGEP(cpu_times, Arg);
         Loads[Idx] = Builder.CreateLoad(C, TypeNames[Idx]);
      }
      Value* S = Builder.CreateMul(ConstantInt::get(I32Ty, Num), Loads[Idx]/*ConstantInt::get(I32Ty,1)*/);
      Sum = Builder.CreateAdd(Sum, S);
   }
   return Sum;
}

