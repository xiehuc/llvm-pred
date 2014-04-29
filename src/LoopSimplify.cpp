#include "LoopSimplify.h"

#include <llvm/Support/CommandLine.h>
#include <llvm/Transforms/Utils/LoopUtils.h>

#include <ValueProfiling.h>

#include <list>

#include "loop.h"
#include "util.h"
#include "config.h"
#include "Resolver.h"
#include "SlashShrink.h"

using namespace std;
using namespace lle;
using namespace llvm;

char LoopCycleSimplify::ID = 0;

static RegisterPass<LoopCycleSimplify> Y("loop-cycle-simplify","Loop Cycle Simplify Pass",false,false);

cl::opt<bool> ValueProfiling("insert-value-trap", cl::desc("insert value profiling trap for loop cycle"));

static void tryResolve(Value* V,const Pass* P,raw_ostream& OS = outs())
{
	bool changed = false;
	string Tab(3,' ');
	vector<Value*> resolved;
	list<Value*> unsolved;
	unsolved = lle::resolve(V, resolved);
	list<Value*>::iterator I = unsolved.begin();
	while(I != unsolved.end()){
		changed = false;
		if(!isa<Instruction>(*I)){
			++I;
			continue;
		}
		Instruction* II = dyn_cast<Instruction>(*I);
		SmallVector<lle::FindedDependenciesType,64> Result;
		lle::find_dependencies(II, P, Result);
		for(auto f : Result){
			Instruction* DI = f.first.getInst(); //Dependend Instruction
			if(f.first.isNonLocal()) continue;
			if(f.first.isClobber()){
				if(!HIDE_CLOBBER)
					OS<<Tab<<f.first<<" : "<<*DI<<" in '"<<f.second->getName()<<"'\n";
				continue;
			}
			if(f.first.isNonFuncLocal()){
				resolved.push_back(II);
				list<Value*> temp = lle::resolve(II->getOperand(0), resolved);
				unsolved.insert(unsolved.end(),temp.begin(),temp.end());
				changed = true;
				break;
			}
			if(DI == II) continue;
			if(isa<StoreInst>(DI)){
				resolved.push_back(II);
				resolved.push_back(DI);
				list<Value*> temp = lle::resolve(DI->getOperand(0),resolved);
				unsolved.insert(unsolved.end(), temp.begin(), temp.end());
				changed = true;
				break;
			}
         /*else{
           ASSERT(0, DI, "unknow Dependence Instruction Type");
           }*/
		}
		if(changed) I = unsolved.erase(I);
		else ++I;
	}

	OS<<"::resolved list\n";
	for(auto i : resolved)
		OS<<Tab<<*i<<"\n";
	OS<<"::unresolved\n";
	for(auto i : unsolved)
		OS<<Tab<<*i<<"\n";
}

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
	bool changed = false;
	lle::LoopCycle& LC = getAnalysis<lle::LoopCycle>();
	if(L->getLoopPreheader()==NULL){
		InsertPreheaderForLoop(L, this);
		changed = true;
	}
	Value* CC = LC.getLoopCycle(L);
	if(!CC) return changed;

   if(ValueProfiling)
      ValueProfiler::insertValueTrap(CC, L->getLoopPreheader()->getTerminator());

   lle::Resolver R;
   R.resolve(CC);

	return changed;
}

/*bool lle::LoopCycleSimplify::runOnModule(llvm::Module& M)
{
	return false;
}*/

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
		tryResolve(CC, this, OS);
		OS<<"\n";
	}
}
