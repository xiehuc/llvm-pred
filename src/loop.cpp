#include "loop.h"
#include "debug.h"
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>

#include <map>

namespace lle
{
	using namespace llvm;

	DenseMap<llvm::Loop*, Loop::Storage> Loop::stores;


	Value* Loop::insertLoopCycle()
	{
		// inspired from Loop::getCanonicalInductionVariable
		BasicBlock *H = getHeader();

		outs()<<"loop simplify:"<<this->isLoopSimplifyForm()<<"\n";
		DEBUG(if(!isLoopSimplifyForm()) return NULL);
		assert(isLoopSimplifyForm() && "why is not simplify form");

		SmallVector<Edge,8> Exits;
		getExitEdges(Exits);

		for(auto Exit : Exits){
			//this is a true exit
			if(getLoopLatch() == Exit.first)
				break;
		}
		//process true exit

		Value* start = NULL;
		PHINode* ind = NULL;
		Instruction* next = NULL;
		BasicBlock* TE = getLoopLatch();//True Exit
		for(auto I = H->begin();isa<PHINode>(I);++I){
			PHINode* P = cast<PHINode>(I);
			if(start) assert("there are more than one induction,Why???");
			start = P->getIncomingValueForBlock(getLoopPreheader());
			next = dyn_cast<Instruction>(P->getIncomingValueForBlock(getLoopLatch()));
			ind = P;
		}

		DEBUG(if(!start) H->print(outs()));
		assert(start && "couldn't find a start value");
		//process complex loops later
		DEBUG(if(this->getLoopDepth()>1 || !this->getSubLoops().empty()) return NULL);
		DEBUG(outs()<<"loop  depth:"<<this->getLoopDepth()<<"\n");
		DEBUG(outs()<<"start value:"<<*start<<"\n");
		DEBUG(outs()<<"ind   value:"<<*ind<<"\n");
		DEBUG(outs()<<"next  value:"<<*next<<"\n");

		int count = 0;
		for( auto I = ind->use_begin(),E = ind->use_end();I!=E;++I){
			outs()<<**I<<"\n";
			++count;
		}
		outs()<<count<<"\n";

		//process non add later
		DEBUG(if(next->getOpcode() != Instruction::Add) return NULL);
		assert(next->getOpcode() == Instruction::Add && "why induction increment is not Add");
		DEBUG(if(next->getOperand(0) != ind) return NULL);
		assert(next->getOperand(0) == ind && "why induction increment is not add it self");
		assert(dyn_cast<ConstantInt>(next->getOperand(1))->equalsInt(1) && "why induction increment number is not 1");


		DEBUG(if(TE->getTerminator()->getOpcode() != Instruction::Br) return NULL);
		assert(TE->getTerminator()->getOpcode() == Instruction::Br);
		const BranchInst* EBR = cast<BranchInst>(TE->getTerminator());
		assert(EBR->isConditional());
		ICmpInst* EC = dyn_cast<ICmpInst>(EBR->getCondition());
		assert(EC->getUnsignedPredicate() == EC->ICMP_EQ);
		Value* END = EC->getOperand(1);

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
		return stores[this].cycle = RES;
	}

	//may not be constant
	Value* Loop::getInductionStartValue()
	{
		// inspired from Loop::getCanonicalInductionVariable
		BasicBlock *H = getHeader();

		BasicBlock *Incoming = 0, *Backedge = 0;
		pred_iterator PI = pred_begin(H);
		assert(PI != pred_end(H) &&
				"Loop must have at least one backedge!");
		Backedge = *PI++;
		if (PI == pred_end(H)) return 0;  // dead loop
		Incoming = *PI++;
		if (PI != pred_end(H)) return 0;  // multiple backedges?

		if (contains(Incoming)) {
			if (contains(Backedge))
				return 0;
			std::swap(Incoming, Backedge);
		} else if (!contains(Backedge))
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
		PHINode* ind = getCanonicalInductionVariable();
		ind->print(outs());
		if(ind == NULL) return NULL;
		//i++
		Value* indpp = ind->getIncomingValueForBlock(getLoopLatch());
		for(auto ii = indpp->use_begin(), ee = indpp->use_end();ii!=ee;++ii){
			Instruction* inst = dyn_cast<Instruction>(*ii);
			if(!inst) continue;
			if(inst->getOpcode() == Instruction::ICmp){
				Value* v1 = inst->getOperand(0);
				Value* v2 = inst->getOperand(1);
				if(v1 == indpp && isLoopInvariant(v2)) return v2;
				if(v2 == indpp && isLoopInvariant(v1)) return v1;
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
