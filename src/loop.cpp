#define DEBUG_TYPE "loop-cycle"

#include "loop.h"
#include "util.h"
#include "config.h"
#include "debug.h"

#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/ADT/Statistic.h>

#include <llvm/Transforms/Utils/LoopUtils.h>

#include <ValueProfiling.h>

#include <map>
#include <vector>
#include <algorithm>

using namespace std;
using namespace lle;
using namespace llvm;

char lle::LoopCycle::ID = 0;
char lle::LoopCycleSimplify::ID = 0;

static RegisterPass<LoopCycle> X("loop-cycle","Loop Cycle Pass",false,true);
static RegisterPass<LoopCycleSimplify> Y("loop-cycle-simplify","Loop Cycle Simplify Pass",false,false);

STATISTIC(NumUnfoundCycle, "Number of unfound loop cycle");

//cl::opt<bool> PrettyPrint("pretty-print", cl::desc("pretty print loop cycle"));
cl::opt<bool> ValueProfiling("insert-value-trap", cl::desc("insert value profiling trap for loop cycle"));


//find start value fron induction variable
static Value* tryFindStart(PHINode* IND,Loop* L,BasicBlock*& StartBB)
{
	if(L->getLoopPredecessor()){ 
		StartBB = L->getLoopPredecessor();
		return IND->getIncomingValueForBlock(StartBB);
	} else {
		Value* start = NULL;
		for(int I = 0,E = IND->getNumIncomingValues();I!=E;++I){
			if(L->contains(IND->getIncomingBlock(I))) continue;
			//if there are many entries, assume they are all equal
			//****??? should do castoff ???******
			if(start && start != IND->getIncomingValue(I)) return NULL;
			start = IND->getIncomingValue(I);
			StartBB = IND->getIncomingBlock(I);
		}
		return start;
	}
}

static void tryResolve(Value* V,const Pass* P,raw_ostream& OS = outs())
{
	bool changed = false;
	string Tab(3,' ');
	vector<Value*> resolved;
	list<Instruction*> unsolved;
	unsolved = lle::resolve(V, resolved);
	list<Instruction*>::iterator I = unsolved.begin();
	while(I != unsolved.end()){
		changed = false;
		SmallVector<lle::FindedDependenciesType,64> Result;
		OS<<"::possible dependence for "<<**I<<"\n";
		lle::find_dependencies(*I, P, Result);
		for(auto f : Result){
			Instruction* DI = f.first.getInst(); //Dependend Instruction
			if(f.first.isNonLocal()) continue;
			if(f.first.isClobber()){
				if(!HIDE_CLOBBER)
					OS<<Tab<<f.first<<" : "<<*DI<<" in '"<<f.second->getName()<<"'\n";
				continue;
			}
			if(DI == *I) continue;
			if(isa<StoreInst>(DI)){
				resolved.push_back(*I);
				resolved.push_back(DI);
				list<Instruction*> temp = lle::resolve(DI->getOperand(0),resolved);
				unsolved.insert(unsolved.end(), temp.begin(), temp.end());
				changed = true;
				break;
			}/*else{
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

void lle::LoopCycle::getAnalysisUsage(llvm::AnalysisUsage & AU) const
{
	AU.setPreservesAll();
	//AU.addRequired<LoopInfo>();
	//setPreservesCFG would block llvm-loop
}

Value* lle::LoopCycle::insertLoopCycle(Loop* L)
{
	// inspired from Loop::getCanonicalInductionVariable
	BasicBlock *H = L->getHeader();
	BasicBlock* LoopPred = L->getLoopPredecessor();
	BasicBlock* startBB = NULL;//which basicblock stores start value
	int OneStep = 0;// the extra add or plus step for calc

	DEBUG(outs()<<"loop simplify:"<<L->isLoopSimplifyForm()<<"\n");
	DEBUG(outs()<<"loop  depth:"<<L->getLoopDepth()<<"\n");
	/** whats difference on use of predecessor and preheader??*/
	//RET_ON_FAIL(self->getLoopLatch()&&self->getLoopPreheader());
	//assert(self->getLoopLatch() && self->getLoopPreheader() && "need loop simplify form" );
	RET_ON_FAIL(L->getLoopLatch());
	assert(L->getLoopLatch() && "need loop simplify form" );

	BasicBlock* TE = NULL;//True Exit
	SmallVector<BasicBlock*,4> Exits;
	L->getExitingBlocks(Exits);

	if(Exits.size()==1) TE = Exits.front();
	else{
		if(std::find(Exits.begin(),Exits.end(),L->getLoopLatch())!=Exits.end()) TE = L->getLoopLatch();
		else{
			SmallVector<llvm::Loop::Edge,4> ExitEdges;
			L->getExitEdges(ExitEdges);
			//stl 用法,先把所有满足条件的元素(出口的结束符是不可到达)移动到数组的末尾,再统一删除
			ExitEdges.erase(std::remove_if(ExitEdges.begin(), ExitEdges.end(), 
						[](llvm::Loop::Edge& I){
						return isa<UnreachableInst>(I.second->getTerminator());
						}), ExitEdges.end());
			if(ExitEdges.size()==1) TE = const_cast<BasicBlock*>(ExitEdges.front().first);
		}
	}

	//process true exit
	RET_ON_FAIL(TE);
	assert(TE && "need have a true exit");

	Instruction* IndOrNext = NULL;
	Value* END = NULL;

	if(isa<BranchInst>(TE->getTerminator())){
		const BranchInst* EBR = cast<BranchInst>(TE->getTerminator());
		RET_ON_FAIL(EBR->isConditional());
		assert(EBR->isConditional());
		ICmpInst* EC = dyn_cast<ICmpInst>(EBR->getCondition());
		RET_ON_FAIL(EC->getPredicate() == EC->ICMP_EQ || EC->getPredicate() == EC->ICMP_SGT);
		assert(VERBOSE(EC->getPredicate() == EC->ICMP_EQ || EC->getPredicate() == EC->ICMP_SGT,EC) && "why end condition is not ==");
		if(EC->getPredicate() == EC->ICMP_SGT) OneStep += 1;
		IndOrNext = dyn_cast<Instruction>(castoff(EC->getOperand(0)));
		END = EC->getOperand(1);
		DEBUG(outs()<<"end   value:"<<*EC<<"\n");
	}else if(isa<SwitchInst>(TE->getTerminator())){
		SwitchInst* ESW = const_cast<SwitchInst*>(cast<SwitchInst>(TE->getTerminator()));
		IndOrNext = dyn_cast<Instruction>(castoff(ESW->getCondition()));
		for(auto I = ESW->case_begin(),E = ESW->case_end();I!=E;++I){
			if(!L->contains(I.getCaseSuccessor())){
				RET_ON_FAIL(!END);
				assert(!END && "shouldn't have two ends");
				END = I.getCaseValue();
			}
		}
		DEBUG(outs()<<"end   value:"<<*ESW<<"\n");
	}else{
		RET_ON_FAIL(0);
		assert(0 && "unknow terminator type");
	}

	RET_ON_FAIL(L->isLoopInvariant(END));
	assert(L->isLoopInvariant(END) && "end value should be loop invariant");


	Value* start = NULL;
	Value* ind = NULL;
	Instruction* next = NULL;
	bool addfirst = false;//add before icmp ed

	DISABLE(outs()<<*IndOrNext<<"\n");
	if(isa<LoadInst>(IndOrNext)){
		//memory depend analysis
		Value* PSi = IndOrNext->getOperand(0);//point type Step.i

		int SICount[2] = {0};//store in predecessor count,store in loop body count
		for( auto I = PSi->use_begin(),E = PSi->use_end();I!=E;++I){
			DISABLE(outs()<<**I<<"\n");
			StoreInst* SI = dyn_cast<StoreInst>(*I);
			if(!SI || SI->getOperand(1) != PSi) continue;
			if(!start&&L->isLoopInvariant(SI->getOperand(0))) {
				if(SI->getParent() != LoopPred)
					if(std::find(pred_begin(LoopPred),pred_end(LoopPred),SI->getParent()) == pred_end(LoopPred)) continue;
				start = SI->getOperand(0);
				startBB = SI->getParent();
				++SICount[0];
			}
			Instruction* SI0 = dyn_cast<Instruction>(SI->getOperand(0));
			if(L->contains(SI) && SI0 && SI0->getOpcode() == Instruction::Add){
				next = SI0;
				++SICount[1];
			}

		}
		RET_ON_FAIL(SICount[0]==1 && SICount[1]==1);
		assert(SICount[0]==1 && SICount[1]==1);
		ind = IndOrNext;
	}else{
		if(isa<PHINode>(IndOrNext)){
			PHINode* PHI = cast<PHINode>(IndOrNext);
			ind = IndOrNext;
			if(castoff(PHI->getIncomingValue(0)) == castoff(PHI->getIncomingValue(1)) && PHI->getParent() != H)
				ind = castoff(PHI->getIncomingValue(0));
			addfirst = false;
		}else if(IndOrNext->getOpcode() == Instruction::Add){
			next = IndOrNext;
			addfirst = true;
		}else{
			RET_ON_FAIL(0);
			assert(0 && "unknow how to analysis");
		}

		for(auto I = H->begin();isa<PHINode>(I);++I){
			PHINode* P = cast<PHINode>(I);
			if(ind && P == ind){
				//start = P->getIncomingValueForBlock(L->getLoopPredecessor());
				start = tryFindStart(P, L, startBB);
				next = dyn_cast<Instruction>(P->getIncomingValueForBlock(L->getLoopLatch()));
			}else if(next && P->getIncomingValueForBlock(L->getLoopLatch()) == next){
				//start = P->getIncomingValueForBlock(L->getLoopPredecessor());
				start = tryFindStart(P, L, startBB);
				ind = P;
			}
		}
	}


	RET_ON_FAIL(start);
	assert(start && "couldn't find a start value");
	//process complex loops later
	//DEBUG(if(L->getLoopDepth()>1 || !L->getSubLoops().empty()) return NULL);
	DEBUG(outs()<<"start value:"<<*start<<"\n");
	DEBUG(outs()<<"ind   value:"<<*ind<<"\n");
	DEBUG(outs()<<"next  value:"<<*next<<"\n");


	//process non add later
	unsigned next_phi_idx = 0;
	ConstantInt* Step = NULL,*PrevStep = NULL;/*only used if next is phi node*/
	PHINode* next_phi = dyn_cast<PHINode>(next);
	do{
		if(next_phi) {
			next = dyn_cast<Instruction>(next_phi->getIncomingValue(next_phi_idx));
			RET_ON_FAIL(next);
			DEBUG(outs()<<"next phi "<<next_phi_idx<<":"<<*next<<"\n");
			if(Step&&PrevStep){
				RET_ON_FAIL(Step->getSExtValue() == PrevStep->getSExtValue());
				assert(Step->getSExtValue() == PrevStep->getSExtValue());
			}
			PrevStep = Step;
		}
		RET_ON_FAIL(next->getOpcode() == Instruction::Add);
		assert(next->getOpcode() == Instruction::Add && "why induction increment is not Add");
		RET_ON_FAIL(next->getOperand(0) == ind);
		assert(next->getOperand(0) == ind && "why induction increment is not add it self");
		Step = dyn_cast<ConstantInt>(next->getOperand(1));
		RET_ON_FAIL(Step);
		assert(Step);
	}while(next_phi && ++next_phi_idx<next_phi->getNumIncomingValues());
	//RET_ON_FAIL(Step->equalsInt(1));
	//assert(VERBOSE(Step->equalsInt(1),Step) && "why induction increment number is not 1");


	Value* RES = NULL;
	//if there are no predecessor, we can insert code into start value basicblock
	BasicBlock* insertBB = LoopPred?:startBB;
	IRBuilder<> Builder(insertBB->getTerminator());
	assert(start->getType()->isIntegerTy() && END->getType()->isIntegerTy() && " why increment is not integer type");
	if(start->getType() != END->getType()){
		start = Builder.CreateCast(CastInst::getCastOpcode(start, false,
					END->getType(), false),start,END->getType());
	}
	if(Step->isMinusOne())
		RES = Builder.CreateSub(start,END);
	else//Step Couldn't be zero
		RES = Builder.CreateSub(END, start);
	if(addfirst) OneStep -= 1;
	if(Step->isMinusOne()) OneStep*=-1;
	assert(OneStep<=1 && OneStep>=-1);
	RES = (OneStep==1)?Builder.CreateAdd(RES,Step):(OneStep==-1)?Builder.CreateSub(RES, Step):RES;
	if(!Step->isMinusOne()&&!Step->isOne())
		RES = Builder.CreateSDiv(RES, Step);
	RES->setName("loop-cycle");

	return CycleMap[L] = RES;
}

bool lle::LoopCycle::runOnLoop(llvm::Loop* L,llvm::LPPassManager&)
{
	CurL = L;
	Function* CurF = L->getHeader()->getParent();
	Value* CC = insertLoopCycle(L);
	if(!CC){
		++NumUnfoundCycle;
		++::NumUnfoundCycle;
		unfound<<"Function:"<<CurF->getName()<<"\n";
		unfound<<"\t"<<*L<<"\n";
		return false;
	}

	if(isa<Constant>(CC)) 
		return false;
	return true;
}

lle::LoopCycle::~LoopCycle()
	//we have no place to print out statistics information
{
	if(NumUnfoundCycle){
		errs()<<std::string(73,'*')<<"\n";
		errs()<<"\tNote!! there are "<<NumUnfoundCycle<<" loop cycles unresolved:\n";
		errs()<<unfound.str();
		errs()<<std::string(73,'*')<<"\n";
	}
}

void lle::LoopCycle::print(llvm::raw_ostream& OS,const llvm::Module*) const
{
	OS<<*CurL<<"\n";
	OS<<"Cycle:"<<*getLoopCycle(CurL)<<"\n";
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
	if(CC){
		if(ValueProfiling)
			ValueProfiler::insertValueTrap(CC, L->getLoopPreheader()->getTerminator());
	}
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
