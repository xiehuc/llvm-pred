#include "loop.h"
#include "debug.h"
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>

#include <map>

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

	Value* Loop::insertLoopCycle()
	{
		// inspired from Loop::getCanonicalInductionVariable
		BasicBlock *H = self->getHeader();

		outs()<<"loop simplify:"<<self->isLoopSimplifyForm()<<"\n";
		//DEBUG(if(!isLoopSimplifyForm()) return NULL);
		//assert(isLoopSimplifyForm() && "why is not simplify form");
		RET_ON_FAIL(self->getLoopLatch()&&self->getLoopPreheader());
		assert(self->getLoopLatch() && self->getLoopPreheader() && "need loop simplify form" );

		SmallVector<llvm::Loop::Edge,4> Exits;
		self->getExitEdges(Exits);

		BasicBlock* TE = NULL;//True Exit
		for(auto Exit : Exits){
			//this is a true exit
			if(self->getLoopLatch() == Exit.first){
				TE = const_cast<BasicBlock*>(Exit.first);
				break;
			}
		}
		//process true exit
		RET_ON_FAIL(TE);
		assert(TE && "need have a true exit");

		DEBUG(if(TE->getTerminator()->getOpcode() != Instruction::Br) return NULL);
		assert(TE->getTerminator()->getOpcode() == Instruction::Br);
		const BranchInst* EBR = cast<BranchInst>(TE->getTerminator());
		assert(EBR->isConditional());
		ICmpInst* EC = dyn_cast<ICmpInst>(EBR->getCondition());
		DEBUG(if(EC->getUnsignedPredicate() != EC->ICMP_EQ) return NULL);
		assert(VERBOSE(EC->getUnsignedPredicate() == EC->ICMP_EQ,EC) && "why end condition is not ==");

		Value* start = NULL;
		PHINode* ind = dyn_cast<PHINode>(castoff(EC->getOperand(0)));
		Instruction* next = NULL;
		for(auto I = H->begin();isa<PHINode>(I);++I){
			PHINode* P = cast<PHINode>(I);
			if(P == ind){
				start = P->getIncomingValueForBlock(self->getLoopPreheader());
				next = dyn_cast<Instruction>(P->getIncomingValueForBlock(self->getLoopLatch()));
			}
		}

		RET_ON_FAIL(start);
		assert(start && "couldn't find a start value");
		//process complex loops later
		//DEBUG(if(self->getLoopDepth()>1 || !self->getSubLoops().empty()) return NULL);
		DEBUG(outs()<<"loop  depth:"<<self->getLoopDepth()<<"\n");
		DEBUG(outs()<<"start value:"<<*start<<"\n");
		DEBUG(outs()<<"ind   value:"<<*ind<<"\n");
		DEBUG(outs()<<"next  value:"<<*next<<"\n");

		int count = 0;
		for( auto I = ind->use_begin(),E = ind->use_end();I!=E;++I){
			outs()<<**I<<"\n";
			++count;
		}
		outs()<<count<<"\n";

		Value* END = EC->getOperand(1);
		//process non add later
		DEBUG(if(next->getOpcode() != Instruction::Add) return NULL);
		assert(next->getOpcode() == Instruction::Add && "why induction increment is not Add");
		DEBUG(if(next->getOperand(0) != ind) return NULL);
		assert(next->getOperand(0) == ind && "why induction increment is not add it self");
		ConstantInt* PLUS = dyn_cast<ConstantInt>(next->getOperand(1));
		DEBUG(if(!PLUS) return NULL);
		assert(PLUS);
		DEBUG(if(!PLUS->equalsInt(1))return NULL);
		assert(VERBOSE(PLUS->equalsInt(1),PLUS) && "why induction increment number is not 1");


		IRBuilder<> Builder(H->getFirstInsertionPt());
		Value* RES = NULL;
		assert(start->getType()->isIntegerTy() && END->getType()->isIntegerTy() && " why increment is not integer type");
		RES = Builder.CreateSub(EC->getOperand(1), start);
		/*
		   Value* LHS = Builder.CreateUIToFP(END, Type::getFloatTy(H->getContext()));
		   Value* RHS = Builder.CreateUIToFP(start, Type::getFloatTy(H->getContext()));
		   RES = Builder.CreateFSub(LHS, RHS);
		   */
		//insert the result to last second instruction
		new BitCastInst(RES,RES->getType(),"loop",const_cast<BranchInst*>(EBR));
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
