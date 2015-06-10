#include "preheader.h"
#include <llvm/IR/Module.h>
#include <llvm/Support/GraphWriter.h>
#include <llvm/ADT/DepthFirstIterator.h>
#include <ValueProfiling.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Support/raw_ostream.h>

#include <iostream>
#include <list>

#include "ddg.h"
#include "util.h"
#include "Reduce.h"
#include "config.h"
#include "Resolver.h"
#include "LoopTripCount.h"

#include "debug.h"

namespace lle{
   class InsertLoopTripCount:public llvm::FunctionPass
   {
      LoopTripCount* LTC;
      //ResolverPass* RP;
      public:
      static char ID;
      explicit InsertLoopTripCount():FunctionPass(ID){}
      void getAnalysisUsage(llvm::AnalysisUsage&) const override;
      bool runOnFunction(llvm::Function& F) override;
      bool runOnLoop(llvm::Loop* L);
      void print(llvm::raw_ostream&,const llvm::Module*) const override;
   };
};

using namespace std;
using namespace lle;
using namespace llvm;

char InsertLoopTripCount::ID = 0;

static RegisterPass<InsertLoopTripCount> Y("Insert-Trip-Count", "Insert Loop Trip Count into Module", true, true);

void InsertLoopTripCount::getAnalysisUsage(llvm::AnalysisUsage & AU) const
{
  AU.setPreservesAll();
  AU.addRequired<LoopInfo>();
  //AU.addRequired<ResolverPass>();
  AU.addRequired<LoopTripCount>();
  AU.addRequired<ScalarEvolution>();
}

bool InsertLoopTripCount::runOnLoop(llvm::Loop *L)
{
	Value* CC = LTC->getOrInsertTripCount(L);
   
	if(!CC) return false;

   // auto insert value trap when used -insert-value-profiling
   CC = ValueProfiler::insertValueTrap(CC, L->getLoopPreheader()->getTerminator());

   /*auto R = RP->getResolverSet<UseOnlyResolve, SpecialResolve>();
   ResolveResult RR = R.resolve(CC);

   if(Ddg && std::get<0>(RR).size()>1){
      string loop_name;
      raw_string_ostream os(loop_name);
      L->print(os);

      StringRef func_name = L->getHeader()->getParent()->getName();

      DDGraph d(RR, CC);
      WriteGraph(&d, func_name+"-ddg", false, os.str());
   }
   */
	return false;
}

bool InsertLoopTripCount::runOnFunction(Function &F)
{
   auto& LI = getAnalysis<LoopInfo>();
   LTC = &getAnalysis<LoopTripCount>();
   //RP = &getAnalysis<ResolverPass>();
   bool Changed = false;
   LTC->updateCache(LI);

   for(Loop* Top : LI)
      for(auto L = df_begin(Top), E = df_end(Top); L!=E; ++L){
         Changed |= runOnLoop(*L);
      }

   return Changed;
}

void InsertLoopTripCount::print(llvm::raw_ostream &OS, const llvm::Module *M) const
{
   auto& LI = getAnalysis<LoopInfo>();

   for(Loop* Top : LI)
      for(auto L = df_begin(Top), E = df_end(Top); L!=E; ++L){
         Value* CC = LTC->getTripCount(*L);
         if(!CC) continue;
         OS<<**L;
         OS<<"Cycles:"<<*CC<<"\n\n";

#if 0
         lle::Resolver<UseOnlyResolve> R; /* print is not a part of normal process
                                             . so don't make it modify resolver cache*/
         ResolveResult RR = R.resolve(CC);
         OS<<**L;
         OS<<"Cycles:";
#ifdef CYCLE_EXPR_USE_DDG
         DDGraph d(RR, CC);
         OS<<d.expr();
#else
         lle::pretty_print(CC, OS);
#endif
         OS<<"\n";
         OS<<"resolved:\n";
         for( auto V : get<0>(RR) ){
            if(isa<Function>(V)) continue;
            OS<<*V<<"\n";
         }
         if(!get<1>(RR).empty()){
            OS<<"unresolved:\n";
            for( auto V : get<1>(RR) ){
               OS<<*V<<"\n";
            }
         }
         OS<<"\n\n";
#endif
      }
   OS<<"\n\n";
}
