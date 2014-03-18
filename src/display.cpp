#include "display.h"
#include "debug.h"

#include <set>
#include <string>

#include <llvm/IR/Constants.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Instructions.h>

using namespace lle;
using namespace llvm;

//phinode circle recursion visit magic word
#define PHINODE_CIRCLE "Δ"


static void pretty_print(BinaryOperator* bin,raw_ostream& o)
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
	pretty_print(bin->getOperand(0),o);
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
		o<<" "<<symbols.at(bin->getOpcode())<<" ";

	pretty_print(bin->getOperand(1),o);
}

static void pretty_print(CmpInst* cmp,raw_ostream& o)
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
	pretty_print(cmp->getOperand(0),o);
	o<<" "<<symbols.at(cmp->getPredicate())<<" ";

	pretty_print(cmp->getOperand(1),o);
}

static void pretty_print(Constant* c,raw_ostream& o)
{
	if(isa<ConstantInt>(c))
		cast<ConstantInt>(c)->getValue().print(o, true);
	else if(isa<ConstantFP>(c))
		o<< cast<ConstantFP>(c)->getValueAPF().convertToDouble();
	else if(isa<GlobalValue>(c))
		o<<"@"<<c->getName();
	else if(isa<ConstantExpr>(c)){
		ConstantExpr* CExp = cast<ConstantExpr>(c);
		lle::pretty_print(CExp->getAsInstruction(),o);
	}else{
		o<<*c;
		//ASSERT(0,*c,"not defined Constant print");
	}
}

static void pretty_print(PHINode* PH,raw_ostream& o)
{
	static SmallVector<PHINode*, 8> visitStack;
	//if we found a circle recursion, we must pass it
	if(std::find(visitStack.begin(), visitStack.end(), PH) != visitStack.end()){
		o<<PHINODE_CIRCLE;
		return;
	}
	std::set<std::string> StrS;//str set
	visitStack.push_back(PH);
	std::string Lstr;
	for(int i=0,e=PH->getNumIncomingValues();i!=e;++i){
		std::string Rstr;
		raw_string_ostream Collect(Rstr);
		lle::pretty_print(PH->getIncomingValue(i),Collect);
		Collect.flush();
		if(Rstr == PHINODE_CIRCLE) continue;
		//if(Lstr==""){ Lstr = Rstr; continue; }
		//ASSERT(Lstr==Rstr,"Lstr:"+Lstr+",Rstr:"+Rstr,"PHINode all incoming values should be same");
		StrS.insert(Rstr);
	}
	if(StrS.size()==1){
		o<<*StrS.begin();
	}else{
		o<<"{"<<*StrS.begin()<<"}";
		for(auto I=++StrS.begin(),E=StrS.end();I!=E;++I)
			o<<"||{"<<*I<<"}";
	}
	//o<<Lstr;
	visitStack.pop_back();
}

void lle::pretty_print(Value* v,raw_ostream& o)
{
	if(isa<Constant>(v)){
		::pretty_print(cast<Constant>(v),o);
		return;
	}
	if(isa<Argument>(v))
		o<<"%"<<v->getName();
	Instruction* inst = dyn_cast<Instruction>(v);
	if(!inst) return;

	if(inst->isBinaryOp())
		::pretty_print(cast<BinaryOperator>(inst),o);
	else if(isa<CmpInst>(inst))
		::pretty_print(cast<CmpInst>(inst),o);
	else if(isa<PHINode>(inst))
		::pretty_print(cast<PHINode>(inst),o);
	else if(isa<LoadInst>(inst)||isa<StoreInst>(inst)){
		o<<"*";
		lle::pretty_print(inst->getOperand(0),o);
	}else if(isa<AllocaInst>(inst))
		o<<"%"<<inst->getName();
	else if(isa<SelectInst>(inst)){
		o<<"(";
		lle::pretty_print(inst->getOperand(0),o);
		o<<")? ";
		lle::pretty_print(inst->getOperand(1),o);
		o<<" : ";
		lle::pretty_print(inst->getOperand(2),o);
		ASSERT(inst->getNumOperands() == 3, *inst, "select should have only 3 operant");
	}
	else if(isa<CastInst>(inst)){
		o<<"(";
		CastInst* c = cast<CastInst>(inst);
		c->getDestTy()->print(o);
		o<<")(";
		lle::pretty_print(c->getOperand(0),o);
		o<<")";
	}
	else if(isa<GetElementPtrInst>(inst)){
		lle::pretty_print(inst->getOperand(0),o);
		for(unsigned i=1;i<inst->getNumOperands();i++){
			o<<"[";
			lle::pretty_print(inst->getOperand(i),o);
			o<<"]";
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
