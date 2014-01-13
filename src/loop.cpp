#include "loop.h"
#include <llvm/IR/InstrTypes.h>

#include <map>

namespace ll {

	using namespace llvm;

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


	void pretty_print(Value* v)
	{
		//v->print(outs());outs()<<"\n";
		if(isa<Constant>(v)){
			v->print(outs());
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
}
