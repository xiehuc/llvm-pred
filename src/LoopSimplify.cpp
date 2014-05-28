#include "LoopSimplify.h"

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/GraphWriter.h>

#include <ValueProfiling.h>

#include <list>

#include "ddg.h"
#include "loop.h"
#include "util.h"
#include "config.h"
#include "Resolver.h"
#include "SlashShrink.h"

#include "debug.h"

using namespace std;
using namespace lle;
using namespace llvm;

char LoopCycleSimplify::ID = 0;

static RegisterPass<LoopCycleSimplify> Y("loop-cycle-simplify","Loop Cycle Simplify Pass",false,false);

void lle::LoopCycleSimplify::getAnalysisUsage(llvm::AnalysisUsage & AU) const
{
	AU.setPreservesAll();
	AU.addRequired<AliasAnalysis>();
	AU.addRequired<MemoryDependenceAnalysis>();
   AU.addRequired<ResolverPass>();
	AU.addRequired<LoopCycle>();
}

bool lle::LoopCycleSimplify::runOnLoop(llvm::Loop *L, llvm::LPPassManager & LPM)
{
	CurL = L;
	LoopCycle& LC = getAnalysis<LoopCycle>();
   ResolverPass& RP = getAnalysis<ResolverPass>();
	Value* CC = LC.getLoopCycle(L);
	if(!CC) return false;

   // auto insert value trap when used -insert-value-profiling
   CC = ValueProfiler::insertValueTrap(CC, L->getLoopPreheader()->getTerminator());

   auto& R = RP.getResolver<UseOnlyResolve>();
   ResolveResult RR = R.resolve(CC, [](Value* V){
         if(Instruction* I = dyn_cast<Instruction>(V))
            MarkPreserve::mark(I, "loop");
         });
   for( auto V : get<1>(RR) )
      MarkPreserve::mark_all<NoResolve>(V, "loop");

   DDGraph d(get<0>(RR), get<1>(RR), get<2>(RR), CC);
   WriteGraph(&d, "test.dot");
	return false;
}

void lle::LoopCycleSimplify::print(llvm::raw_ostream &OS, const llvm::Module *) const
{
	auto& LC = getAnalysis<lle::LoopCycle>();
	Value* CC = LC.getLoopCycle(CurL);
	if(CC){
		OS<<"in Function:\t"<<CurL->getHeader()->getParent()->getName()<<"\n";
		OS<<*CurL;
		OS<<"Cycles:";
		lle::pretty_print(CC, OS);
      OS<<"\n";
      lle::Resolver<UseOnlyResolve> R; /* print is not a part of normal process
         . so don't make it modify resolver cache*/
      OS<<"resolved:\n";
      lle::ResolveResult RR = R.resolve(CC, [&OS](Value* V){
            if(isa<Function>(V)) return;
            OS<<*V<<"\n";
            });
      if(!get<1>(RR).empty()){
         OS<<"unresolved:\n";
         for( auto V : get<1>(RR) ){
            OS<<*V<<"\n";
         }
      }
		OS<<"\n\n";
	}
}
