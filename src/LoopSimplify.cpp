#include "LoopSimplify.h"

#include <llvm/IR/Module.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/GraphWriter.h>

#include <ProfileInfo.h>
#include <ValueProfiling.h>

#include <list>

#include "ddg.h"
#include "loop.h"
#include "util.h"
#include "GVInfo.h"
#include "config.h"
#include "Resolver.h"
#include "SlashShrink.h"

#include "debug.h"

using namespace std;
using namespace lle;
using namespace llvm;

char LoopCycleSimplify::ID = 0;

static cl::opt<bool> Ddg("Ddg", cl::desc("Draw Data Dependencies Graph"));
static RegisterPass<LoopCycleSimplify> Y("loop-cycle-simplify","Loop Cycle Simplify Pass",false,false);

void lle::LoopCycleSimplify::getAnalysisUsage(llvm::AnalysisUsage & AU) const
{
	AU.setPreservesAll();
   AU.addRequired<GVInfo>();
	AU.addRequired<AliasAnalysis>();
	AU.addRequired<MemoryDependenceAnalysis>();
   AU.addRequired<ResolverPass>();
	AU.addRequired<LoopCycle>();
   AU.addRequired<ProfileInfo>();
}

bool lle::LoopCycleSimplify::runOnLoop(llvm::Loop *L, llvm::LPPassManager & LPM)
{
	CurL = L;
	LoopCycle& LC = getAnalysis<LoopCycle>();
   ResolverPass& RP = getAnalysis<ResolverPass>();
   ProfileInfo& PI = getAnalysis<ProfileInfo>();
   GVInfo& GVI = getAnalysis<GVInfo>();
	Value* CC = LC.getLoopCycle(L);
   
   DEBUG(errs()<<"[Load ProfileInfo, Traped size:"<<PI.getAllTrapedValues().size()<<"]\n");

	if(!CC) return false;

   // auto insert value trap when used -insert-value-profiling
   CC = ValueProfiler::insertValueTrap(CC, L->getLoopPreheader()->getTerminator());

   RP.getResolver<SLGResolve>().get_impl().initial(&PI);
   RP.getResolver<GVResolve>().get_impl().initial(&GVI);
   auto R = RP.getResolverSet<UseOnlyResolve, SpecialResolve, GVResolve, SLGResolve>();
   ResolveResult RR = R.resolve(CC, [](Value* V){
         if(Instruction* I = dyn_cast<Instruction>(V))
            MarkPreserve::mark(I, "loop");
         });
   for( auto V : get<1>(RR) )
      MarkPreserve::mark_all<NoResolve>(V, "loop");

   if(Ddg && get<0>(RR).size()>1){
      string loop_name;
      raw_string_ostream os(loop_name);
      L->print(os);

      StringRef func_name = L->getHeader()->getParent()->getName();

      DDGraph d(RR, CC);
      WriteGraph(&d, func_name+"-ddg", false, os.str());
   }
	return false;
}

void lle::LoopCycleSimplify::print(llvm::raw_ostream &OS, const llvm::Module *M) const
{
	auto& LC = getAnalysis<lle::LoopCycle>();
	Value* CC = LC.getLoopCycle(CurL);
	if(CC){
      lle::Resolver<UseOnlyResolve> R; /* print is not a part of normal process
         . so don't make it modify resolver cache*/
      ResolveResult RR = R.resolve(CC);
		OS<<"in Function:\t"<<CurL->getHeader()->getParent()->getName()<<"\n";
		OS<<*CurL;
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
	}
}
