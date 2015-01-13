#include "preheader.h"
#include <llvm/Analysis/BranchProbabilityInfo.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/CFG.h>

#include <deque>
#include <PredBlockProfiling.h>

#include "BlockFreqExpr.h"
#include "SlashShrink.h"
#include "Resolver.h"
#include "ddg.h"
#include "debug.h"


namespace lle {
   class PerformPred;
};

class lle::PerformPred : public llvm::FunctionPass
{
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
   llvm::DenseMap<llvm::Instruction*, llvm::Value*> Promoted;
   llvm::BranchProbabilityInfo* BPI;

   public:
   static char ID;
   PerformPred():llvm::FunctionPass(ID) {}
   bool runOnFunction(llvm::Function& F) override;
   void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
   void print(llvm::raw_ostream&,const llvm::Module*) const override;

   private:
   llvm::Value* count(llvm::BasicBlock& BB, llvm::IRBuilder<>& Builder);
   llvm::BasicBlock* promote(llvm::Instruction* LoopTC);
   llvm::BasicBlock* findCriticalBlock(llvm::BasicBlock* From, llvm::BasicBlock* To);
   llvm::BranchProbability getPathProbability(llvm::BasicBlock* From, llvm::BasicBlock* To);
};

using namespace llvm;
using namespace lle;

char PerformPred::ID = 0;
static RegisterPass<PerformPred> X("PerfPred", "get Performance Predication Model");

inline Value* force_insert(llvm::Value* V, IRBuilder<>& Builder, const Twine& Name="")
{
   return isa<Constant>(V)?CastInst::CreateSExtOrBitCast(V, V->getType(), Name, Builder.GetInsertPoint()):V;
}

static Value* CreateMul(IRBuilder<>& Builder, Value* TripCount, BranchProbability prob)
{
   static double square = std::sqrt(INT32_MAX);
   prob = scale(prob);
   uint32_t n = prob.getNumerator(), d = prob.getDenominator();
   if(n == d) return TripCount; /* TC * 1.0 */
   Type* I32Ty = Type::getInt32Ty(TripCount->getContext());
   Type* FloatTy = Type::getFloatTy(TripCount->getContext());
   Value* Ret = TripCount;
   if(n>square){
      // it may overflow, use float to caculate
      Ret = Builder.CreateFMul(Builder.CreateSIToFP(Ret, FloatTy), ConstantFP::get(FloatTy, (double)n/d));
      return Builder.CreateFPToSI(Ret, I32Ty);
   }
   if(n!=1) Ret = Builder.CreateMul(Ret, ConstantInt::get(I32Ty, n));
   return Builder.CreateSDiv(Ret, ConstantInt::get(I32Ty, d));
}

Value* selectBranch(IRBuilder<>& Builder, Value* True, BasicBlock* From, BasicBlock* To)
{
   if(From == NULL || To == NULL || From == To) return True;
   Value* False = ConstantInt::get(True->getType(), 0);
   auto Term = From->getTerminator();
   unsigned N = Term->getNumSuccessors();
   if(N < 2) return True;
   auto Path = getPath(From, To);
   int succ = -1;
   for(unsigned i = 0; i<N; ++i){
      BasicBlock* Succ = Term->getSuccessor(i);
      if(find(Path.begin(), Path.end(), Succ) != Path.end()){
         succ = i;
         break;
      }
   }
   errs()<<From->getName()<<":"<<succ<<"\n";
   if(succ!=-1){
      if(BranchInst* Br = dyn_cast<BranchInst>(Term)){
         if(succ == 0) return Builder.CreateSelect(Br->getCondition(), True, False);
         else return Builder.CreateSelect(Br->getCondition(), False, True);
      }
   }
   return True;
}

void PerformPred::getAnalysisUsage(AnalysisUsage &AU) const
{
   AU.addRequired<BlockFreqExpr>();
   AU.addRequired<DominatorTreeWrapperPass>();
   AU.addRequired<BranchProbabilityInfo>();
   AU.setPreservesCFG();
}
BasicBlock* PerformPred::findCriticalBlock(BasicBlock *From, BasicBlock *To)
{
   /** From !dom \phi
    *  \phi^-1 dom From where \phi^-1 == idom \phi
    */
   DominatorTree& DomT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
   BasicBlock* Last = NULL;
   do{
      Last = To;
      To = DomT.getNode(To)->getIDom()->getBlock();
   }while(!DomT.dominates(To, From));
   return Last;
}

BasicBlock* PerformPred::promote(Instruction* LoopTC)
{
   DominatorTree& DomT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();

   SmallVector<Instruction*, 4> depends;
   SmallVector<Instruction*, 4> targets;
   targets.push_back(LoopTC);

   unsigned Idx = 0;
   while(Idx != targets.size()){
      Instruction* I = targets[Idx];
      for(auto O = I->op_begin(), E = I->op_end(); O!=E; ++O){
         if(Instruction* OI = dyn_cast<Instruction>(O->get())){
            if(O->get()->getNumUses() > 1)
               depends.push_back(OI);
            else{
               targets.push_back(OI);
            }
         }
      }
      ++Idx;
   }

   BasicBlock* InsertInto, *dep;
   Assert(depends.size() > 0,"");
   InsertInto = depends.front()->getParent();
   if(depends.size()>1){
      /* let C is insert point, 
       * dep1 dom C, dep2 dom C => dep1 dom dep2 or dep2 dom dep1 */
      for(auto Ite = depends.begin()+1, E = depends.end(); Ite != E; ++Ite){
         dep = (*Ite)->getParent();
         InsertInto = DomT.dominates(InsertInto, dep)?dep:InsertInto;
      }
   }
   if(LoopTC->getParent() == InsertInto) return InsertInto;

   for(auto I = targets.rbegin(), E = targets.rend(); I!=E; ++I)
      (*I)->moveBefore(InsertInto->getTerminator());
   return InsertInto;
}

BranchProbability PerformPred::getPathProbability(BasicBlock *From, BasicBlock *To)
{
   BranchProbability empty(1,1);
   if(From == NULL || To == NULL || From == To) return empty;
   auto Path = getPath(From, To);
   if(Path.size()<2) return empty;
   size_t n=1,d=1;
   for(unsigned i = 0, e = Path.size()-1; i<e; ++i){
      BranchProbability p2 = BPI->getEdgeProbability(Path[i], Path[i+1]);
      n *= p2.getNumerator();
      d *= p2.getDenominator();
   }
   return scale(BranchProbability(n,d));
}

bool PerformPred::runOnFunction(Function &F)
{
   if( F.isDeclaration() ) return false;
   Promoted.clear();
   BPI = &getAnalysis<BranchProbabilityInfo>();
   BlockFreqExpr& BFE = getAnalysis<BlockFreqExpr>();
   DominatorTree& DomT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
   pred_cls.clear();
   Type* I32Ty = Type::getInt32Ty(F.getContext());
   errs()<<F.getName()<<"\n";

   IRBuilder<> Builder(F.getEntryBlock().getTerminator());
   Value* SumLhs = NULL, *SumRhs = NULL;
   BlockFrequency EntryFreq = BFE.getBlockFreq(&F.getEntryBlock());

   for(auto& BB : F){
      auto FreqExpr = BFE.getBlockFreqExpr(&BB);
      if(FreqExpr.second != NULL) continue;
      // process other blocks outside loops
      SumRhs = count(BB, Builder);
      // XXX use scale in caculate
      BranchProbability freq = BFE.getBlockFreq(&BB)/EntryFreq;
      SumRhs = CreateMul(Builder, SumRhs, freq);
      SumRhs->setName(BB.getName()+".bfreq");
      SumRhs = force_insert(SumRhs, Builder, BB.getName()+".bfreq");
      PredBlockProfiler::increaseBlockCounter(&BB, SumRhs, Builder.GetInsertPoint());
      MarkPreserve::mark(dyn_cast<Instruction>(SumRhs));
      pred_cls.emplace_back(BFE.inLoop(&BB)?STATIC_LOOP:STATIC_BLOCK, &BB, freq);
   }

   SmallVector<Instruction*, 20> BfreqStack;
   if(SumLhs)BfreqStack.push_back(dyn_cast<Instruction>(SumLhs)); // the last instr in entry block
   for(auto& BB : F){
      auto FreqExpr = BFE.getBlockFreqExpr(&BB);
      Value* LoopTC = FreqExpr.second;
      if(LoopTC == NULL) continue;
      // process all loops
      if(Instruction* LoopTCI = dyn_cast<Instruction>(FreqExpr.second)){
         auto found = Promoted.find(LoopTCI);
         if(found == Promoted.end()){
            BasicBlock* Ori = LoopTCI->getParent();
            AssertRuntime(Ori->getParent()==&F, "");
            BasicBlock* Pro = promote(LoopTCI);
            // promote insert point
            Builder.SetInsertPoint(Pro->getTerminator());
#if defined(PROMOTE_FREQ_path_prob)
            LoopTC = CreateMul(Builder, LoopTC, getPathProbability(Pro, Ori));
#endif
#if defined(PROMOTE_FREQ_select)
            LoopTC = selectBranch(Builder, LoopTC, Pro, Ori);
#endif
            Promoted[LoopTCI] = LoopTC;
         }else{
            Builder.SetInsertPoint(LoopTCI->getParent()->getTerminator());
            LoopTC = found->second;
         }
      }else
         Builder.SetInsertPoint(F.getEntryBlock().getTerminator());
      SumRhs = count(BB, Builder);
      if(LoopTC->getType()!= I32Ty)
         LoopTC = Builder.CreateCast(Instruction::SExt, LoopTC, I32Ty);
      Value* freq = CreateMul(Builder, LoopTC, FreqExpr.first); // \psi * \frac {bfreq_LLVM(N)} {bfreq_LLVM(H)}
      SumRhs = Builder.CreateMul(freq, SumRhs); // cost = freq * count
      SumRhs->setName(BB.getName()+".bfreq");
      if(ConstantInt* CI = dyn_cast<ConstantInt>(SumRhs)){
         pred_cls.emplace_back(&BB, CI->getSExtValue()); /* DYNAMIC_LOOP_CONST */
      }else
         pred_cls.emplace_back(&BB, SumRhs); /* DYNAMIC_LOOP_INST */

      force_insert(SumRhs, Builder, BB.getName()+".bfreq");
      PredBlockProfiler::increaseBlockCounter(&BB, SumRhs, Builder.GetInsertPoint());
   }
   PrintSum = SumLhs;

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
   OS<<"# Block Sum:"<<*PrintSum<<"\n";
   OS<<"\n";

}


llvm::Value* PerformPred::count(BasicBlock& BB, IRBuilder<>& Builder)
{
   auto I32Ty = Type::getInt32Ty(BB.getContext());
   return ConstantInt::get(I32Ty,1);
}


