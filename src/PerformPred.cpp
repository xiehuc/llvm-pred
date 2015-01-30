#include "preheader.h"
#include <llvm/Analysis/BranchProbabilityInfo.h>
#include <llvm/Analysis/BlockFrequencyInfo.h>
#include <llvm/Support/Format.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Metadata.h>

#include "PredBlockProfiling.h"
#include "LoopTripCount.h"
#include "util.h"
#include "debug.h"


namespace lle {
   class PerformPred;
};

class lle::PerformPred : public llvm::FunctionPass
{
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
   llvm::DenseMap<llvm::Loop*, std::pair<llvm::BasicBlock*, llvm::Value*> > ViewPort; // Header -> (E_P, B_P,N)
   llvm::BranchProbabilityInfo* BPI;
   llvm::BlockFrequencyInfo* BFI;
   llvm::LoopInfo* LI;
   LoopTripCount* LTC;
   llvm::DominatorTree* DomT;

   public:
   static char ID;
   PerformPred():llvm::FunctionPass(ID) {}
   bool runOnFunction(llvm::Function& F) override;
   void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
   void print(llvm::raw_ostream&,const llvm::Module*) const override;

   private:
   llvm::BasicBlock* promote(llvm::Instruction* LoopTC, llvm::Loop* L);
   llvm::BasicBlock* findCriticalBlock(llvm::BasicBlock* From, llvm::BasicBlock* To);
   llvm::BranchProbability getPathProb(llvm::BasicBlock* From, llvm::BasicBlock* To);
   llvm::BranchProbability getPathProb(llvm::BasicBlock *From, llvm::BlockFrequency To);
   llvm::BlockFrequency in_freq(llvm::Loop* L);
};

using namespace llvm;
using namespace lle;

char PerformPred::ID = 0;
static RegisterPass<PerformPred> X("PerfPred", "get Performance Predication Model");

static Value* force_insert(llvm::Value* V, IRBuilder<>& Builder, const Twine& Name="")
{
   if(isa<Constant>(V) || V->hasName())
      return CastInst::CreateSExtOrBitCast(V, V->getType(), Name, Builder.GetInsertPoint());
   else 
      V->setName(Name);
   return V;
}

template<class T>
static llvm::Value* one(T& t)
{
   auto I32Ty = Type::getInt32Ty(t.getContext());
   return ConstantInt::get(I32Ty,1);
}
// integer log2 of a float
static inline int32_t ilog2(float x)
{
	uint32_t ix = (uint32_t&)x;
	uint32_t exp = (ix >> 23) & 0xFF;
	int32_t log2 = int32_t(exp) - 127;

	return log2;
}
// http://stereopsis.com/log2.html
// x should > 0
static inline int32_t ilog2(uint32_t x) {
	return ilog2((float)x);
}

static uint32_t GCD(uint32_t A, uint32_t B) // 最大公约数
{
   uint32_t Max=A, Min=B;
   A<B?Max=B,Min=A:0;
   do{
      A=Min,B=Max%Min;
      A<B?Max=B,Min=A:Max=A,Min=B;
   }while(B!=0);
   return A;
}
static BranchProbability scale(const BranchProbability& prob)
{
   uint32_t n = prob.getNumerator(), d = prob.getDenominator();
   uint32_t gcd = GCD(n,d);
   return BranchProbability(n/gcd, d/gcd);
}
static BranchProbability operator/(const BlockFrequency& LHS, const BlockFrequency& RHS)
{
   uint64_t n = LHS.getFrequency(), d = RHS.getFrequency();
   if(n > UINT32_MAX || d > UINT32_MAX){
      unsigned bit = std::max(ilog2(uint32_t(n>>32)),ilog2(uint32_t(d>>32)))+1;
      n >>= bit;
      d >>= bit;
   }
   return scale(BranchProbability(n, d));
}

static Value* CreateMul(IRBuilder<>& Builder, Value* TripCount, BranchProbability prob)
{
   static double square = std::sqrt(INT32_MAX);
   uint32_t n = prob.getNumerator(), d = prob.getDenominator();
   if(n == d) return TripCount; /* TC * 1.0 */
   Type* I32Ty = Type::getInt32Ty(TripCount->getContext());
   Type* FloatTy = Type::getDoubleTy(TripCount->getContext());
   Value* Ret = TripCount;
   if(n>square){
      // it may overflow, use float to caculate
      Ret = Builder.CreateFMul(Builder.CreateSIToFP(Ret, FloatTy), ConstantFP::get(FloatTy, (double)n/d));
      std::string hint;
      raw_string_ostream(hint)<<"hint."<<n<<"."<<d<<"."<<format("%.3f",(float)n/d);
      LLVMContext& C = Builder.getContext();
      cast<Instruction>(Ret)->setMetadata(hint, MDNode::get(C, MDString::get(C,"lle")));
      return Builder.CreateFPToSI(Ret, I32Ty);
   }
   if(n!=1) Ret = Builder.CreateMul(Ret, ConstantInt::get(I32Ty, n));
   return Builder.CreateSDiv(Ret, ConstantInt::get(I32Ty, d));
}

static Value* selectBranch(IRBuilder<>& Builder, Value* True, BasicBlock* From, BasicBlock* To)
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
   AU.addRequired<LoopInfo>();
   AU.addRequired<LoopTripCount>();
   AU.addRequired<DominatorTreeWrapperPass>();
   AU.addRequired<BranchProbabilityInfo>();
   AU.addRequired<BlockFrequencyInfo>();
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

BasicBlock* PerformPred::promote(Instruction* LoopTC, Loop* L)
{
   SmallVector<BasicBlock*, 4> depends;
   SmallVector<Instruction*, 4> targets;

   if(LoopTC) targets.push_back(LoopTC);
   unsigned Idx = 0;
   while(Idx != targets.size()){
      Instruction* I = targets[Idx];
      for(auto O = I->op_begin(), E = I->op_end(); O!=E; ++O){
         if(Instruction* OI = dyn_cast<Instruction>(O->get())){
            if(O->get()->getNumUses() > 1)
               depends.push_back(OI->getParent());
            else{
               targets.push_back(OI);
            }
         }
      }
      ++Idx;
   }
   Loop* PL = L->getParentLoop();
   if(PL) depends.push_back(ViewPort[PL].first);
   

   BasicBlock* InsertInto, *dep;
   if(depends.empty())
      return &L->getHeader()->getParent()->getEntryBlock();

   InsertInto = depends.front();
   if(depends.size()>1){
      /* let C is insert point, 
       * dep1 dom C, dep2 dom C => dep1 dom dep2 or dep2 dom dep1 */
      for(auto Ite = depends.begin()+1, E = depends.end(); Ite != E; ++Ite){
         dep = *Ite;
         InsertInto = DomT->dominates(InsertInto, dep)?dep:InsertInto;
      }
   }

   if(targets.size() > 0 && targets.front()->getParent() != InsertInto){
      for(auto I = targets.rbegin(), E = targets.rend(); I!=E; ++I)
         (*I)->moveBefore(InsertInto->getTerminator());
   }
   return InsertInto;
}

BranchProbability PerformPred::getPathProb(BasicBlock *From, BasicBlock *To)
{
   BranchProbability empty(1,1);
   if(From == NULL || To == NULL || From == To) return empty;
   Loop* F_L = LI->getLoopFor(From), *T_L = LI->getLoopFor(To);
   if(F_L == T_L) // they are in same loop level
      return BFI->getBlockFreq(To)/BFI->getBlockFreq(From);
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
BranchProbability PerformPred::getPathProb(BasicBlock *From, BlockFrequency To)
{
   //Assume they are in same loop level
   //(F-->T) = bfreq_LLVM(T)/bfreq_LLVM(F)
   return scale(To/BFI->getBlockFreq(From));
}

BlockFrequency PerformPred::in_freq(Loop* L)
{
   BasicBlock* P = L->getLoopPreheader();
   if(P) return BFI->getBlockFreq(P);
   BlockFrequency in;
   BasicBlock* H = L->getHeader();
   for(auto P = pred_begin(H), E = pred_end(H); P!=E; ++P){
      BasicBlock* Pred = *P;
      if(L->contains(Pred)) continue;
      in += BFI->getBlockFreq(Pred) * BPI->getEdgeProbability(Pred, H);
   }
   return in;
}

bool PerformPred::runOnFunction(Function &F)
{
   if( F.isDeclaration() ) return false;
   Promoted.clear();
   ViewPort.clear();
   BPI = &getAnalysis<BranchProbabilityInfo>();
   LI = &getAnalysis<LoopInfo>();
   LTC = &getAnalysis<LoopTripCount>();
   BFI = &getAnalysis<BlockFrequencyInfo>();
   DomT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
   pred_cls.clear();
   LTC->updateCache(*LI);

   IRBuilder<> Builder(F.getEntryBlock().getTerminator());
   Value* SumRhs = NULL;
   BasicBlock* Entry = &F.getEntryBlock();

   Loop* L;
   Value* TC;
   std::vector<BasicBlock*> AllBlocks;
   pushback_to(F.begin(), F.end(), AllBlocks);

   for(auto BB : AllBlocks){
      L = LI->getLoopFor(BB);
      TC = L?LTC->getOrInsertTripCount(L):NULL;
      if(TC!=NULL) continue;
      // process other blocks outside loops
      BranchProbability freq = getPathProb(Entry, BB);
      SumRhs = CreateMul(Builder, one(*BB), freq); //(\[GothicE]-->BB)
      SumRhs = force_insert(SumRhs, Builder, BB->getName()+".bfreq");
      PredBlockProfiler::increaseBlockCounter(BB, SumRhs, Builder.GetInsertPoint());
      pred_cls.emplace_back(LI->getLoopFor(BB)?STATIC_LOOP:STATIC_BLOCK, BB, freq);
   }

   for(auto BB : AllBlocks){
      L = LI->getLoopFor(BB);
      TC = L?LTC->getOrInsertTripCount(L):NULL;
      Value* freq;
      if(TC==NULL) continue;

      if(BB == L->getHeader()){
         BlockFrequency P = in_freq(L); // in_freq(L) == bfreq(Preheader)
         Loop* PL = L->getParentLoop();
         BasicBlock* E_V = promote(dyn_cast<Instruction>(TC), L);// view point
         Builder.SetInsertPoint(E_V->getTerminator());
         Value* B_PN = NULL;
         if(PL==NULL){// TC(V-->P)
#if defined(PROMOTE_FREQ_path_prob)
            B_PN = CreateMul(Builder, TC, getPathProb(E_V, P)); 
#endif
#if defined(PROMOTE_FREQ_select)
            B_PN = selectBranch(Builder, TC, E_V, L->getHeader());
#endif
         }else{
            BasicBlock* E_Vn = ViewPort[PL].first; // parent loop's view point
            if(E_Vn == E_V){
               B_PN = ViewPort[PL].second;// parent loop's view probability
               B_PN = CreateMul(Builder, B_PN, getPathProb(PL->getHeader(), P)); // B_PN (H-->P)
               B_PN = Builder.CreateMul(B_PN, TC); // B_PN (H--P) %tc
            }else{
               B_PN = one(*BB);
               for(PL = L->getParentLoop(); PL==NULL||PL->contains(E_V); L = PL){
                  B_PN = Builder.CreateMul(B_PN, LTC->getOrInsertTripCount(L));
                  B_PN = CreateMul(Builder, B_PN, getPathProb(PL->getHeader(), in_freq(L)));
               }
               B_PN = Builder.CreateMul(B_PN, LTC->getOrInsertTripCount(L));
               B_PN = CreateMul(Builder, B_PN, getPathProb(E_V, in_freq(L)));
            }
         }
         ViewPort[L] = std::make_pair(E_V, B_PN);
         freq = B_PN;
      }else{
         auto View = ViewPort[L];
         Builder.SetInsertPoint(View.first->getTerminator());
         freq = CreateMul(Builder, View.second, getPathProb(L->getHeader(), BB));
      }
      SumRhs = force_insert(freq, Builder, BB->getName()+".bfreq");
      if(ConstantInt* CI = dyn_cast<ConstantInt>(SumRhs)){
         pred_cls.emplace_back(BB, CI->getSExtValue()); /* DYNAMIC_LOOP_CONST */
      }else
         pred_cls.emplace_back(BB, SumRhs); /* DYNAMIC_LOOP_INST */

      PredBlockProfiler::increaseBlockCounter(BB, SumRhs, Builder.GetInsertPoint());
   }

   return true;
}

void PerformPred::print(llvm::raw_ostream & OS, const llvm::Module * M) const
{
   static size_t idx = 0;
   for(auto classify : pred_cls){
      OS<<"No."<<idx++<<"\t\""<<classify.block->getName()<<"\"\t";
      switch(classify.type){
         case STATIC_BLOCK: 
         case STATIC_LOOP:  
            OS<<(classify.type==STATIC_BLOCK?"Static Block:\t":"Static Loop Block:\t")<<
               classify.data.freq<<"\n";
            break;
         case DYNAMIC_LOOP_CONST: 
            OS<<"Dynamic Loop Constant Trip Count:\t"<<
               classify.data.ext_val<<"\n"; 
            break;
         case DYNAMIC_LOOP_INST:  
            OS<<"Dynamic Loop Instrumented Trip Count\t"<<
               *classify.data.value<<"\n"; 
            break;
      }
   }
   OS<<"\n";
}

