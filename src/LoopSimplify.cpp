#include "LoopSimplify.h"

#include <llvm/Support/CommandLine.h>

#include <ValueProfiling.h>

#include <list>

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

cl::opt<bool> ValueProfiling("Value-Trap", cl::desc("insert value profiling trap for loop cycle"));

void lle::LoopCycleSimplify::getAnalysisUsage(llvm::AnalysisUsage & AU) const
{
	AU.setPreservesAll();
	AU.addRequired<LoopInfo>();
	AU.addRequired<AliasAnalysis>();
	AU.addRequired<MemoryDependenceAnalysis>();
	AU.addRequired<lle::LoopCycle>();
}

bool lle::LoopCycleSimplify::runOnLoop(llvm::Loop *L, llvm::LPPassManager & LPM)
{
	CurL = L;
	lle::LoopCycle& LC = getAnalysis<lle::LoopCycle>();
	Value* CC = LC.getLoopCycle(L);
	if(!CC) return false;

   if(ValueProfiling)
      CC = ValueProfiler::insertValueTrap(CC, L->getLoopPreheader()->getTerminator());

   lle::Resolver<UseOnlyResolve> R;
   ResolveResult RR = R.resolve(CC, [](Value* V){
         if(Instruction* I = dyn_cast<Instruction>(V))
            MarkPreserve::mark(I, "loop");
         });
   for( auto V : get<1>(RR) )
      MarkPreserve::mark_all<NoResolve>(V, "loop");

	return false;
}

void lle::LoopCycleSimplify::print(llvm::raw_ostream &OS, const llvm::Module *) const
{
	auto& LC = getAnalysis<lle::LoopCycle>();
	Value* CC = LC.getLoopCycle(CurL);
	if(CC){
		OS<<"in Function:\t"<<CurL->getHeader()->getParent()->getName()<<"\n";
		OS<<*CurL<<"\n";
		OS<<"Cycles:";
		lle::pretty_print(CC, OS);
		OS<<"\n";
#if 0
		tryResolve(CC, this, OS);
		OS<<"\n";
#endif
	}
}
