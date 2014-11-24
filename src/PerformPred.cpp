#include "preheader.h"
#include <llvm/Analysis/PostDominators.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/GraphWriter.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/CFG.h>

#include <deque>
#include <ValueProfiling.h>

#include "BlockFreqExpr.h"
#include "Resolver.h"
#include "ddg.h"
#include "debug.h"


namespace lle {
   class PerformPred;
};

class lle::PerformPred : public llvm::FunctionPass
{
   llvm::GlobalVariable* cpu_times = NULL;
   llvm::Value* PrintSum; // a sum value used for print
   enum ClassifyType {
      STATIC_BLOCK,        // 静态预测的普通基本块
      STATIC_LOOP,         // 静态预测的循环
      DYNAMIC_LOOP_CONST,  // 动态预测的循环次数常量
      DYNAMIC_LOOP_INST,   // 动态预测的循环次数非常量
   };
   struct Classify {
      Classify(ClassifyType t, llvm::BasicBlock* b, llvm::BranchProbability p):type(t),block(b) {
         Assert(t<DYNAMIC_LOOP_CONST, " only accept static prediction type");
         data.freq = (double)p.getNumerator()/p.getDenominator();
      }
      Classify(llvm::BasicBlock* b, int64_t v):type(DYNAMIC_LOOP_CONST),block(b) { data.ext_val = v;}
      Classify(llvm::BasicBlock* b, llvm::Value* v):type(DYNAMIC_LOOP_INST),block(b) { data.value = v;}
      ClassifyType type;
      llvm::BasicBlock* block;
      union {
         double freq; // 静态预测会给出频率
         int64_t ext_val; // 动态预测常量比静态预测更准确
         llvm::Value* value; // 动态预测变量是一个Value
      }data;
   };
   std::vector<Classify> pred_cls; // 对每个block进行分类, 并且储存关键的属性.

   public:
   static char ID;
   PerformPred():llvm::FunctionPass(ID) {}
   bool runOnFunction(llvm::Function& F) override;
   //bool runOnSCC(llvm::CallGraphSCC& SCC) override;
   void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
   void print(llvm::raw_ostream&,const llvm::Module*) const override;

   private:
   llvm::Value* cost(llvm::BasicBlock& BB, llvm::IRBuilder<>& Builder);
   llvm::BasicBlock* promote(llvm::Instruction* LoopTC);
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

inline Value* force_insert(llvm::Value* V, IRBuilder<>& Builder, const Twine& Name="")
{
   return isa<Constant>(V)?CastInst::CreateSExtOrBitCast(V, V->getType(), Name, Builder.GetInsertPoint()):V;
}


static Value* CreateMul(IRBuilder<>& Builder, Value* TripCount, BranchProbability prob)
{
   prob = scale(prob);
   uint32_t n = prob.getNumerator(), d = prob.getDenominator();
   if(n == d) return TripCount; /* TC * 1.0 */
   Type* I32Ty = Type::getInt32Ty(TripCount->getContext());
   Type* FloatTy = Type::getFloatTy(TripCount->getContext());
   Value* Ret = TripCount;
   double square = std::sqrt(INT32_MAX);
   if(n>square){
      // it may overflow, use float to caculate
      Ret = Builder.CreateFMul(Builder.CreateSIToFP(Ret, FloatTy), ConstantFP::get(FloatTy, (double)n/d));
      return Builder.CreateFPToSI(Ret, I32Ty);
   }
   if(n!=1) Ret = Builder.CreateMul(Ret, ConstantInt::get(I32Ty, n));
   return Builder.CreateSDiv(Ret, ConstantInt::get(I32Ty, d));
}

void PerformPred::getAnalysisUsage(AnalysisUsage &AU) const
{
   //CallGraphSCCPass::getAnalysisUsage(AU);
   AU.addRequired<BlockFreqExpr>();
   AU.addRequired<DominatorTreeWrapperPass>();
   //AU.addRequired<PostDominatorTree>();
   AU.setPreservesCFG();
}

BasicBlock* PerformPred::promote(Instruction* LoopTC)
{
   DominatorTree& DomT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();

   SmallVector<Instruction*, 4> depends;
   std::deque<Instruction*> targets;
   targets.push_back(LoopTC);

   auto Ite = targets.begin();
   while(Ite != targets.end()){
      Instruction* I = *Ite;
      for(auto O = I->op_begin(), E = I->op_end(); O!=E; ++O){
         if(Instruction* OI = dyn_cast<Instruction>(O->get())){
            if(O->get()->getNumUses() > 1)
               depends.push_back(OI);
            else{
               OI->removeFromParent();
               targets.push_back(OI);
            }
         }
      }
      ++Ite;
   }

   BasicBlock* InsertInto = NULL;
   Assert(depends.size() > 0,"");
   InsertInto = depends.front()->getParent();
   if(depends.size()>1){
      for(auto Ite = depends.begin()+1, E = depends.end(); Ite != E; ++Ite)
         InsertInto = DomT.findNearestCommonDominator(InsertInto, (*Ite)->getParent());
   }
   if(LoopTC->getParent() == InsertInto) return InsertInto;

   for(auto I = targets.rbegin(), E = targets.rend(); I!=E; ++I)
      (*I)->moveBefore(InsertInto->getTerminator());
   return InsertInto;
}

bool PerformPred::runOnFunction(Function &F)
{
   if( F.isDeclaration() ) return false;
   BlockFreqExpr& BFE = getAnalysis<BlockFreqExpr>();
   DominatorTree& DomT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
   //PostDominatorTree& PDomT = getAnalysis<PostDominatorTree>();
   pred_cls.clear();

   if(cpu_times == NULL){
      Type* ETy = Type::getInt32Ty(F.getContext());
      Type* ATy = ArrayType::get(ETy, NumTypes);
      cpu_times = new GlobalVariable(ATy, false, GlobalValue::InternalLinkage, ConstantFP::get(ETy, 1.0L), "cpuTime");
   }

   IRBuilder<> Builder(F.getEntryBlock().getTerminator());
   auto I32Ty = Type::getInt32Ty(F.getContext());
   Value* SumLhs = ConstantInt::get(I32Ty, 0), *SumRhs;
   BlockFrequency EntryFreq = BFE.getBlockFreq(&F.getEntryBlock());
   for(auto& BB : F){
      auto FreqExpr = BFE.getBlockFreqExpr(&BB);
      if(FreqExpr.second != NULL) continue;
      // process other blocks outside loops
      SumRhs = cost(BB, Builder);
      // XXX use scale in caculate
      BranchProbability freq = BFE.getBlockFreq(&BB)/EntryFreq;
      SumRhs = CreateMul(Builder, SumRhs, freq);
      SumRhs->setName(BB.getName()+".bfreq");
#if PRED_TYPE == EXEC_TIME
      SumLhs = Builder.CreateAdd(SumLhs, SumRhs);
      SumLhs->setName(BB.getName()+".sfreq");
#else
      SumRhs = force_insert(SumRhs, Builder, BB.getName()+".bfreq");
      ValueProfiler::insertValueTrap(SumRhs, Builder.GetInsertPoint());
#endif
      pred_cls.emplace_back(BFE.inLoop(&BB)?STATIC_LOOP:STATIC_BLOCK, &BB, freq);
   }

   SmallVector<Instruction*, 20> BfreqStack;
   BfreqStack.push_back(dyn_cast<Instruction>(SumLhs)); // the last instr in entry block
   for(auto& BB : F){
      auto FreqExpr = BFE.getBlockFreqExpr(&BB);
      Value* LoopTC = FreqExpr.second;
      if(LoopTC == NULL) continue;
      // process all loops
      if(Instruction* LoopTCI = dyn_cast<Instruction>(FreqExpr.second)){
         AssertRuntime(LoopTCI->getParent()->getParent()==&F, "");
         Instruction* InsertPos = promote(LoopTCI)->getTerminator();
         // promote insert point
         Builder.SetInsertPoint(InsertPos);
      }else
         Builder.SetInsertPoint(F.getEntryBlock().getTerminator());
      SumRhs = cost(BB, Builder);
      if(LoopTC->getType()!= I32Ty)
         LoopTC = Builder.CreateCast(Instruction::SExt, LoopTC, I32Ty);
      Value* freq = CreateMul(Builder, LoopTC, FreqExpr.first); // trip_count * prob = freq
      SumRhs = Builder.CreateMul(freq, SumRhs);
      SumRhs->setName(BB.getName()+".bfreq");
      if(ConstantInt* CI = dyn_cast<ConstantInt>(SumRhs)){
         pred_cls.emplace_back(&BB, CI->getSExtValue()); /* DYNAMIC_LOOP_CONST */
      }else
         pred_cls.emplace_back(&BB, SumRhs); /* DYNAMIC_LOOP_INST */

#if PRED_TYPE == EXEC_TIME
      Instruction* SumLI = dyn_cast<Instruction>(SumLhs);
      BasicBlock* InsertB = Builder.GetInsertBlock();
      if(DomT.dominates(InsertB, SumLI->getParent())){ 
         //if promote too much, Insert before SumLI, we should demote it.
         Builder.SetInsertPoint(SumLI->getParent()->getTerminator());
         InsertB = Builder.GetInsertBlock();
      }
      if(SumLI && !DomT.dominates(SumLI->getParent(), InsertB) && InsertB->getNumUses()>1){
         //still couldn't dominate all, try create phi instruction
         auto save = Builder.saveIP();
         Builder.SetInsertPoint(InsertB->getFirstNonPHI());
         PHINode* Phi = Builder.CreatePHI(SumLI->getType(), InsertB->getNumUses());
         Builder.restoreIP(save);
         for(auto PI = pred_begin(InsertB), PE = pred_end(InsertB); PI!=PE; ++PI){
            BasicBlock* BB = *PI;
            auto Found = find_if(BfreqStack.rbegin(), BfreqStack.rend(), [BB,&DomT](Instruction* I){
                  return DomT.dominates(I->getParent(), BB);
                  // find prev bfreq that could access
                  });
            Assert(Found != BfreqStack.rend(), "");
            Phi->addIncoming(*Found, BB);
         }
         SumLhs = Phi;
      }

      SumLhs = Builder.CreateAdd(SumLhs, SumRhs);
      SumLhs->setName(BB.getName()+".sfreq");/* sum freq */
      BfreqStack.push_back(dyn_cast<Instruction>(SumLhs));
#else
      force_insert(SumRhs, Builder, BB.getName()+".bfreq");
      ValueProfiler::insertValueTrap(SumRhs, Builder.GetInsertPoint());
#endif
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
   static size_t idx = 0;
   for(auto classify : pred_cls){
      OS<<"No."<<idx++<<"\t";
      switch(classify.type){
         case STATIC_BLOCK: 
         case STATIC_LOOP:  
            OS<<(classify.type==STATIC_BLOCK?"Static Block:\t\"":"Static Loop Block:\t\"")<<
               classify.block->getName()<<"\"\t"<<
               classify.data.freq<<"\n";
            break;
         case DYNAMIC_LOOP_CONST: 
            OS<<"Dynamic Loop Constant Trip Count:\t\""<<
               classify.block->getName()<<"\"\t"<<classify.data.ext_val<<"\n"; 
            break;
         case DYNAMIC_LOOP_INST:  
            OS<<"Dynamic Loop Instrumented Trip Count\t\""<<
               classify.block->getName()<<"\"\t"<<
               *classify.data.value<<"\n"; 
            break;
      }
   }

   lle::Resolver<NoResolve> R;
   auto Res = R.resolve(PrintSum);
   DDGraph ddg(Res, PrintSum);
   OS<<ddg.expr()<<"\n";
}


llvm::Value* PerformPred::cost(BasicBlock& BB, IRBuilder<>& Builder)
{
   auto I32Ty = Type::getInt32Ty(BB.getContext());
#if PRED_TYPE == EXEC_TIME
   unsigned InstCounts[NumTypes] = {0};

   count(BB, InstCounts);
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
#else
   return ConstantInt::get(I32Ty,1);
#endif
}


