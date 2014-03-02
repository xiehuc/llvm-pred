#include "loop.h"
#include "debug.h"
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>

#include <map>
#include <algorithm>

namespace lle
{
	using namespace llvm;

	//remove cast instruction for a value
	//because cast means the original value and the returned value is
	//semanticly equal
	static Value* castoff(Value* v)
	{
		if(CastInst* CI = dyn_cast<CastInst>(v)){
			return castoff(CI->getOperand(0));
		}else
			return v;
	}

	//find start value fron induction variable
	static Value* tryFindStart(PHINode* IND,Loop* l,BasicBlock*& StartBB)
	{
		Loop& L = *l;
		if(L->getLoopPredecessor()){ 
			StartBB = L->getLoopPredecessor();
			return IND->getIncomingValueForBlock(StartBB);
		} else {
			Value* start = NULL;
			for(int I = 0,E = IND->getNumIncomingValues();I!=E;++I){
				if(L->contains(IND->getIncomingBlock(I))) continue;
				//if there are many entries, assume they are all equal
				if(start && start != IND->getIncomingValue(I)) return NULL;
				start = IND->getIncomingValue(I);
				StartBB = IND->getIncomingBlock(I);
			}
			return start;
		}
	}

	Value* Loop::insertLoopCycle()
	{
		// inspired from Loop::getCanonicalInductionVariable
		BasicBlock *H = self->getHeader();
		BasicBlock* LoopPred = self->getLoopPredecessor();
		IRBuilder<> Builder(H->getFirstInsertionPt());
		int OneStep = 0;// the extra add or plus step for calc

		DEBUG(outs()<<"\n");
		DEBUG(outs()<<*loop);
		DEBUG(outs()<<"loop simplify:"<<self->isLoopSimplifyForm()<<"\n");
		DEBUG(outs()<<"loop  depth:"<<self->getLoopDepth()<<"\n");
		/** whats difference on use of predecessor and preheader??*/
		//RET_ON_FAIL(self->getLoopLatch()&&self->getLoopPreheader());
		//assert(self->getLoopLatch() && self->getLoopPreheader() && "need loop simplify form" );
		RET_ON_FAIL(self->getLoopLatch());
		assert(self->getLoopLatch() && "need loop simplify form" );

		BasicBlock* TE = NULL;//True Exit
		SmallVector<BasicBlock*,4> Exits;
		self->getExitingBlocks(Exits);

		if(Exits.size()==1) TE = Exits.front();
		else{
			if(std::find(Exits.begin(),Exits.end(),self->getLoopLatch())!=Exits.end()) TE = self->getLoopLatch();
			else{
				SmallVector<llvm::Loop::Edge,4> ExitEdges;
				self->getExitEdges(ExitEdges);
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
				if(!self->contains(I.getCaseSuccessor())){
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

		RET_ON_FAIL(self->isLoopInvariant(END));
		assert(self->isLoopInvariant(END) && "end value should be loop invariant");


		Value* start = NULL;
		Value* ind = NULL;
		Instruction* next = NULL;
		bool addfirst = false;//add before icmp ed

		outs()<<*IndOrNext<<"\n";
		if(isa<LoadInst>(IndOrNext)){
			//memory depend analysis
			Value* PSi = IndOrNext->getOperand(0);//point type Step.i

			int SICount[2] = {0};//store in predecessor count,store in loop body count
			for( auto I = PSi->use_begin(),E = PSi->use_end();I!=E;++I){
				outs()<<**I<<"\n";
				StoreInst* SI = dyn_cast<StoreInst>(*I);
				if(!SI || SI->getOperand(1) != PSi) continue;
				if(!start&&self->isLoopInvariant(SI->getOperand(0))) {
					if(SI->getParent() != LoopPred)
						if(std::find(pred_begin(LoopPred),pred_end(LoopPred),SI->getParent()) == pred_end(LoopPred)) continue;
					start = SI->getOperand(0);
					startBB = SI->getParent();
					++SICount[0];
				}
				Instruction* SI0 = dyn_cast<Instruction>(SI->getOperand(0));
				if(self->contains(SI) && SI0 && SI0->getOpcode() == Instruction::Add){
					next = SI0;
					++SICount[1];
				}

			}
			outs()<<SICount[0]<<";"<<SICount[1]<<"\n";
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
					//start = P->getIncomingValueForBlock(self->getLoopPredecessor());
					start = tryFindStart(P, this, startBB);
					next = dyn_cast<Instruction>(P->getIncomingValueForBlock(self->getLoopLatch()));
				}else if(next && P->getIncomingValueForBlock(self->getLoopLatch()) == next){
					//start = P->getIncomingValueForBlock(self->getLoopPredecessor());
					start = tryFindStart(P, this, startBB);
					ind = P;
				}
			}
		}


		RET_ON_FAIL(start);
		assert(start && "couldn't find a start value");
		//process complex loops later
		//DEBUG(if(self->getLoopDepth()>1 || !self->getSubLoops().empty()) return NULL);
		DEBUG(outs()<<"start value:"<<*start<<"\n");
		DEBUG(outs()<<"ind   value:"<<*ind<<"\n");
		DEBUG(outs()<<"next  value:"<<*next<<"\n");


		//process non add later
		int next_phi_idx = 0;
		ConstantInt* Step = NULL,*PhiStep = NULL;/*only used if next is phi node*/
		PHINode* next_phi = dyn_cast<PHINode>(next);
		do{
			if(next_phi) {
				next = dyn_cast<Instruction>(next_phi->getIncomingValue(next_phi_idx));
				RET_ON_FAIL(next);
				DEBUG(outs()<<"next phi "<<next_phi_idx<<":"<<*next<<"\n");
				if(Step&&PhiStep){
					RET_ON_FAIL(Step->getSExtValue() == PhiStep->getSExtValue());
					assert(Step->getSExtValue() == PhiStep->getSExtValue());
				}
				PhiStep = Step;
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
		assert(start->getType()->isIntegerTy() && END->getType()->isIntegerTy() && " why increment is not integer type");
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
		/*
		   Value* LHS = Builder.CreateUIToFP(END, Type::getFloatTy(H->getContext()));
		   Value* RHS = Builder.CreateUIToFP(start, Type::getFloatTy(H->getContext()));
		   RES = Builder.CreateFSub(LHS, RHS);
		   */
		//insert the result to last second instruction
		new BitCastInst(RES,RES->getType(),"loop",startBB->getFirstInsertionPt());
		return cycle = RES;
	}

	//may not be constant
	Value* Loop::getInductionStartValue()
	{
		// inspired from Loop::getCanonicalInductionVariable
		BasicBlock *H = self->getHeader();

		BasicBlock *Incoming = 0, *Backedge = 0;
		pred_iterator PI = pred_begin(H);
		assert(PI != pred_end(H) &&
				"Loop must have at least one backedge!");
		Backedge = *PI++;
		if (PI == pred_end(H)) return 0;  // dead loop
		Incoming = *PI++;
		if (PI != pred_end(H)) return 0;  // multiple backedges?

		if (self->contains(Incoming)) {
			if (self->contains(Backedge))
				return 0;
			std::swap(Incoming, Backedge);
		} else if (!self->contains(Backedge))
			return 0;

		Value* candidate = NULL;
		for(auto I = H->begin();isa<PHINode>(I);++I){
			PHINode* P = cast<PHINode>(I);
			if(candidate) assert("there are more than one induction,Why???");
				candidate = P->getIncomingValueForBlock(Incoming);
		}
		return candidate;
	}
	Value* Loop::getCanonicalEndCondition()
	{
		//i
		PHINode* ind = self->getCanonicalInductionVariable();
		ind->print(outs());
		if(ind == NULL) return NULL;
		//i++
		Value* indpp = ind->getIncomingValueForBlock(self->getLoopLatch());
		for(auto ii = indpp->use_begin(), ee = indpp->use_end();ii!=ee;++ii){
			Instruction* inst = dyn_cast<Instruction>(*ii);
			if(!inst) continue;
			if(inst->getOpcode() == Instruction::ICmp){
				Value* v1 = inst->getOperand(0);
				Value* v2 = inst->getOperand(1);
				if(v1 == indpp && self->isLoopInvariant(v2)) return v2;
				if(v2 == indpp && self->isLoopInvariant(v1)) return v1;
			}
		}
		return NULL;
	}
	PHINode* getInductionVariable(llvm::Loop* loop)
	{
	}

	static
		void pretty_print(BinaryOperator* bin)
		{
			static const std::map<int,StringRef> symbols = {
				{Instruction::Add,"+"},
				{Instruction::FAdd,"+"},
				{Instruction::Sub,"-"},
				{Instruction::FSub,"-"},
				{Instruction::Mul,"*"},
				{Instruction::FMul,"*"},
				{Instruction::UDiv,"/"},
				{Instruction::SDiv,"/"},
				{Instruction::FDiv,"/"},
				{Instruction::URem,"%"},
				{Instruction::SRem,"%"},
				{Instruction::FRem,"%"},

				{Instruction::Shl,"<<"},
				{Instruction::LShr,">>"},
				{Instruction::AShr,">>"},

				{Instruction::And,"&"},
				{Instruction::Or,"|"},
				{Instruction::Xor,"^"}
			};
			pretty_print(bin->getOperand(0));
			if(symbols.at(bin->getOpcode())==""){
				errs()<<"unknow operator"<<"\n";
			}
			outs()<<symbols.at(bin->getOpcode());

			pretty_print(bin->getOperand(1));
		}

	static
		void pretty_print(CmpInst* cmp)
		{
			static const std::map<int,StringRef> symbols = {
				{CmpInst::FCMP_FALSE,"false"},
				{CmpInst::FCMP_OEQ,"=="},
				{CmpInst::FCMP_OGT,">"},
				{CmpInst::FCMP_OGE,">="},
				{CmpInst::FCMP_OLT,"<"},
				{CmpInst::FCMP_OLE,"<="},
				{CmpInst::FCMP_ONE,"!="},
				{CmpInst::FCMP_ORD,"??"},
				{CmpInst::FCMP_UNO,"??"},
				{CmpInst::FCMP_UEQ,"=="},
				{CmpInst::FCMP_UGT,">"},
				{CmpInst::FCMP_UGE,">="},
				{CmpInst::FCMP_ULT,"<"},
				{CmpInst::FCMP_ULE,"<="},
				{CmpInst::FCMP_UNE,"!="},
				{CmpInst::FCMP_TRUE,"true"},
				{CmpInst::ICMP_EQ,"=="},
				{CmpInst::ICMP_NE,"!="},
				{CmpInst::ICMP_UGT,">"},
				{CmpInst::ICMP_UGE,">="},
				{CmpInst::ICMP_ULT,"<"},
				{CmpInst::ICMP_ULE,"<="},
				{CmpInst::ICMP_SGT,">"},
				{CmpInst::ICMP_SGE,">="},
				{CmpInst::ICMP_SLT,"<"},
				{CmpInst::ICMP_SLE,"<="},
			};
			pretty_print(cmp->getOperand(0));
			outs()<<symbols.at(cmp->getPredicate());

			pretty_print(cmp->getOperand(1));
		}

	static 
		void pretty_print(Constant* c)
		{
			if(isa<ConstantInt>(c))
				cast<ConstantInt>(c)->getValue().print(outs(), true);
			else if(isa<ConstantFP>(c)){
				outs()<< cast<ConstantFP>(c)->getValueAPF().convertToDouble();
			}else
				c->print(outs());
		}


	void pretty_print(Value* v)
	{
		if(isa<Constant>(v)){
			pretty_print(cast<Constant>(v));
			return;
		}
		Instruction* inst = dyn_cast<Instruction>(v);
		if(!inst) return;
		if(inst->isBinaryOp())
			pretty_print(cast<BinaryOperator>(inst));
		else if(isa<CmpInst>(inst))
			pretty_print(cast<CmpInst>(inst));
		else if(isa<LoadInst>(inst)||isa<StoreInst>(inst))
			outs()<<inst->getOperand(0)->getName();
		else if(isa<SelectInst>(inst)){
			outs()<<"(";
			pretty_print(inst->getOperand(0));
			outs()<<") ? ";
			pretty_print(inst->getOperand(1));
			outs()<<" : ";
			pretty_print(inst->getOperand(2));
		}
		else if(isa<CastInst>(inst)){
			CastInst* c = cast<CastInst>(inst);
			outs()<<"(";
			c->getDestTy()->print(outs());
			outs()<<")";
			pretty_print(c->getOperand(0));
		}
		else if(isa<GetElementPtrInst>(inst)){
			pretty_print(inst->getOperand(0));
			for(int i=1;i<inst->getNumOperands();i++){
				outs()<<"[";
				pretty_print(inst->getOperand(i));
				outs()<<"]";
			}
		}
		else{
			inst->print(outs());outs()<<"\n";
			assert(0 && "not defined instruction print" );
		}

	}


	static
		void latex_print(BinaryOperator* bin)
		{
			static const std::map<int,StringRef> symbols = {
				{Instruction::Add,"+"},
				{Instruction::FAdd,"+"},
				{Instruction::Sub,"-"},
				{Instruction::FSub,"-"},
				{Instruction::Mul,"*"},
				{Instruction::FMul,"*"},
				{Instruction::UDiv,"/"},
				{Instruction::SDiv,"/"},
				{Instruction::FDiv,"/"},
				{Instruction::URem,"%"},
				{Instruction::SRem,"%"},
				{Instruction::FRem,"%"},

				{Instruction::Shl,"<<"},
				{Instruction::LShr,">>"},
				{Instruction::AShr,">>"},

				{Instruction::And,"&"},
				{Instruction::Or,"|"},
				{Instruction::Xor,"^"}
			};
			if(bin->getOpcode()==Instruction::UDiv || bin->getOpcode() == Instruction::SDiv || bin->getOpcode()==Instruction::FDiv){
				outs()<<"\\frac {";
				latex_print(bin->getOperand(0));
				outs()<<"} {";
				latex_print(bin->getOperand(1));
				outs()<<"} ";
				return ;
			}
			pretty_print(bin->getOperand(0));
			if(symbols.at(bin->getOpcode())==""){
				errs()<<"unknow operator"<<"\n";
			}
			outs()<<symbols.at(bin->getOpcode());

			pretty_print(bin->getOperand(1));
		}

	void latex_print(Value* v)
	{
		if(isa<Constant>(v)){
			pretty_print(cast<Constant>(v));
			return;
		}
		Instruction* inst = dyn_cast<Instruction>(v);
		if(!inst) return;
		if(inst->isBinaryOp())
			latex_print(cast<BinaryOperator>(inst));
		else if(isa<CmpInst>(inst))
			pretty_print(cast<CmpInst>(inst));
		else if(isa<LoadInst>(inst)||isa<StoreInst>(inst))
			outs()<<inst->getOperand(0)->getName();
		else if(isa<SelectInst>(inst)){
			outs()<<"(";
			latex_print(inst->getOperand(0));
			outs()<<") ? ";
			latex_print(inst->getOperand(1));
			outs()<<" : ";
			latex_print(inst->getOperand(2));
		}
		else if(isa<CastInst>(inst)){
			CastInst* c = cast<CastInst>(inst);
			outs()<<"(";
			c->getDestTy()->print(outs());
			outs()<<")";
			pretty_print(c->getOperand(0));
		}
		else if(isa<GetElementPtrInst>(inst)){
			pretty_print(inst->getOperand(0));
			for(int i=1;i<inst->getNumOperands();i++){
				outs()<<"[";
				pretty_print(inst->getOperand(i));
				outs()<<"]";
			}
		}
		else{
			inst->print(outs());outs()<<"\n";
			assert(0 && "not defined instruction print" );
		}

	}
}
