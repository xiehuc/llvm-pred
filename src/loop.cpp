#include "loop.h"

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
		else if(isa<LoadInst>(inst)||isa<StoreInst>(inst))
			outs()<<inst->getOperand(0)->getName();
		else{
			inst->print(outs());outs()<<"\n";
			//assert(0 && "not defined instruction print" );
		}

	}
}
