#include "display.h"
#include "debug.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Instructions.h>

#include <llvm/Support/raw_ostream.h>

using namespace lle;
using namespace llvm;


static void pretty_print(BinaryOperator* bin)
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
	//if operand 1 is negative ,should ignore + symbol (+-1)-> (-1)
	bool ignore = false;
	if(bin->getOpcode()==Instruction::Add){
		ConstantInt* CInt = dyn_cast<ConstantInt>(bin->getOperand(1));
		if(CInt && CInt->isNegative()) ignore = true;
		ConstantFP* CFP = dyn_cast<ConstantFP>(bin->getOperand(1));
		if(CFP && CFP->isNegative()) ignore = true;
	}
	if(!ignore)
		outs()<<symbols.at(bin->getOpcode());

	pretty_print(bin->getOperand(1));
}

static void pretty_print(CmpInst* cmp)
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

static void pretty_print(Constant* c)
{
	if(isa<ConstantInt>(c))
		cast<ConstantInt>(c)->getValue().print(outs(), true);
	else if(isa<ConstantFP>(c))
		outs()<< cast<ConstantFP>(c)->getValueAPF().convertToDouble();
	else if(isa<GlobalValue>(c))
		outs()<<"@"<<c->getName();
	else if(isa<ConstantExpr>(c)){
		ConstantExpr* CExp = cast<ConstantExpr>(c);
		lle::pretty_print(CExp->getAsInstruction());
	}else
		c->print(outs());
}


void lle::pretty_print(Value* v)
{
	if(isa<Constant>(v)){
		::pretty_print(cast<Constant>(v));
		return;
	}
	Instruction* inst = dyn_cast<Instruction>(v);
	if(!inst) return;

	if(inst->isBinaryOp())
		::pretty_print(cast<BinaryOperator>(inst));
	else if(isa<CmpInst>(inst))
		::pretty_print(cast<CmpInst>(inst));
	else if(isa<LoadInst>(inst)||isa<StoreInst>(inst))
		lle::pretty_print(inst->getOperand(0));
	else if(isa<AllocaInst>(inst))
		outs()<<"%"<<inst->getName();
	else if(isa<SelectInst>(inst)){
		outs()<<"(";
		lle::pretty_print(inst->getOperand(0));
		outs()<<") ? ";
		lle::pretty_print(inst->getOperand(1));
		outs()<<" : ";
		lle::pretty_print(inst->getOperand(2));
	}
	else if(isa<CastInst>(inst)){
		CastInst* c = cast<CastInst>(inst);
		c->getDestTy()->print(outs());
		outs()<<"(";
		lle::pretty_print(c->getOperand(0));
		outs()<<")";
	}
	else if(isa<GetElementPtrInst>(inst)){
		lle::pretty_print(inst->getOperand(0));
		for(unsigned i=1;i<inst->getNumOperands();i++){
			outs()<<"[";
			lle::pretty_print(inst->getOperand(i));
			outs()<<"]";
		}
	}
	else{
		ASSERT(0,*inst,"not defined instruction print");
	}

}


#if 0
static void latex_print(BinaryOperator* bin)
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
	lle::pretty_print(bin->getOperand(0));
	if(symbols.at(bin->getOpcode())==""){
		errs()<<"unknow operator"<<"\n";
	}
	outs()<<symbols.at(bin->getOpcode());

	lle::pretty_print(bin->getOperand(1));
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
		for(unsigned i=1;i<inst->getNumOperands();i++){
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
#endif
