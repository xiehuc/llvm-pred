#include "display.h"
#include "debug.h"

#include <set>
#include <string>
#include <algorithm>

#include <llvm/IR/Constants.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Instructions.h>
#include <llvm/ADT/SmallVector.h>

using namespace lle;
using namespace llvm;

//phinode circle recursion visit magic word
#define PHINODE_CIRCLE "Δ"
#define LEFT_BRACKET "{"
#define RIGHT_BRACKET "}"
#define PUSH_BRACKETS(precd) if(precd>op_precd.back()) o<<LEFT_BRACKET; op_precd.push_back(precd);
#define POP_BRACKETS(precd) op_precd.pop_back(); if(precd>op_precd.back()) o<<RIGHT_BRACKET;

static SmallVector<int,16> op_precd((unsigned)1,16); // operator precedence with 1 element 16 back

static void pretty_print(BinaryOperator* bin,raw_ostream& o)
{
	static const std::map<int,std::pair<StringRef,int> > symbols = {
		{Instruction::Add,{"+",5}},
		{Instruction::FAdd,{"+",5}},
		{Instruction::Sub,{"-",5}},
		{Instruction::FSub,{"-",5}},
		{Instruction::Mul,{"*",4}},
		{Instruction::FMul,{"*",4}},
		{Instruction::UDiv,{"/",4}},
		{Instruction::SDiv,{"/",4}},
		{Instruction::FDiv,{"/",4}},
		{Instruction::URem,{"%",4}},
		{Instruction::SRem,{"%",4}},
		{Instruction::FRem,{"%",4}},

		{Instruction::Shl,{"<<",6}},
		{Instruction::LShr,{">>",6}},
		{Instruction::AShr,{">>",6}},

		{Instruction::And,{"&",9}},
		{Instruction::Or,{"|",11}},
		{Instruction::Xor,{"^",10}}
	};
	int precd = symbols.at(bin->getOpcode()).second;
	PUSH_BRACKETS(precd);

	pretty_print(bin->getOperand(0),o);
	if(symbols.at(bin->getOpcode()).first==""){
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
		o<<" "<<symbols.at(bin->getOpcode()).first<<" ";
	pretty_print(bin->getOperand(1),o);

	POP_BRACKETS(precd);
}

static void pretty_print(CmpInst* cmp,raw_ostream& o)
{
	static const std::map<int,std::pair<StringRef,int> > symbols = {
		{CmpInst::FCMP_FALSE,{"false",1}},
		{CmpInst::FCMP_OEQ,{"==",8}},
		{CmpInst::FCMP_OGT,{">",7}},
		{CmpInst::FCMP_OGE,{">=",7}},
		{CmpInst::FCMP_OLT,{"<",7}},
		{CmpInst::FCMP_OLE,{"<=",7}},
		{CmpInst::FCMP_ONE,{"!=",8}},
		{CmpInst::FCMP_ORD,{"??",8}},
		{CmpInst::FCMP_UNO,{"??",8}},
		{CmpInst::FCMP_UEQ,{"==",8}},
		{CmpInst::FCMP_UGT,{">",7}},
		{CmpInst::FCMP_UGE,{">=",7}},
		{CmpInst::FCMP_ULT,{"<",7}},
		{CmpInst::FCMP_ULE,{"<=",7}},
		{CmpInst::FCMP_UNE,{"!=",8}},
		{CmpInst::FCMP_TRUE,{"true",1}},
		{CmpInst::ICMP_EQ,{"==",8}},
		{CmpInst::ICMP_NE,{"!=",8}},
		{CmpInst::ICMP_UGT,{">",7}},
		{CmpInst::ICMP_UGE,{">=",7}},
		{CmpInst::ICMP_ULT,{"<",7}},
		{CmpInst::ICMP_ULE,{"<=",7}},
		{CmpInst::ICMP_SGT,{">",7}},
		{CmpInst::ICMP_SGE,{">=",7}},
		{CmpInst::ICMP_SLT,{"<",7}},
		{CmpInst::ICMP_SLE,{"<=",7}},
	};
	int precd = symbols.at(cmp->getPredicate()).second;
	PUSH_BRACKETS(precd);

	pretty_print(cmp->getOperand(0),o);
	o<<" "<<symbols.at(cmp->getPredicate()).first<<" ";
	pretty_print(cmp->getOperand(1),o);

	POP_BRACKETS(precd);
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
	const int precd = 1;
	//if we found a circle recursion, we must pass it
	auto circle = std::find(visitStack.begin(), visitStack.end(), PH);
	if(circle != visitStack.end()){
		o<<PHINODE_CIRCLE"#"<<visitStack.end()-circle;
		return;
	}
	std::set<std::string> StrS;//str set
	visitStack.push_back(PH);
	PUSH_BRACKETS(precd);
	std::string Lstr;
	for(int i=0,e=PH->getNumIncomingValues();i!=e;++i){
		std::string Rstr;
		raw_string_ostream Collect(Rstr);
		lle::pretty_print(PH->getIncomingValue(i),Collect);
		Collect.flush();
		//Δ is 2 bytes, # is 1 byte. if Rstr is Δ#\d+ then we can ignore it
		if((Rstr.compare(0, 3, PHINODE_CIRCLE"#")==0)&&std::all_of(Rstr.begin()+3,Rstr.end(),isdigit)) continue;
		StrS.insert(Rstr);
	}
	if(StrS.size()==1){
		o<<*StrS.begin();
	}else{
		o<<*StrS.begin();
		for(auto I=++StrS.begin(),E=StrS.end();I!=E;++I)
			o<<"||"<<*I;
	}
	POP_BRACKETS(precd);
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
		PUSH_BRACKETS(14);
		o<<"(";
		lle::pretty_print(inst->getOperand(0),o);
		o<<")?";
		lle::pretty_print(inst->getOperand(1),o);
		o<<":";
		lle::pretty_print(inst->getOperand(2),o);
		POP_BRACKETS(14);
		ASSERT(inst->getNumOperands() == 3, *inst, "select should have only 3 operant");
	}
	else if(isa<CastInst>(inst)){
		o<<"(";
		CastInst* c = cast<CastInst>(inst);
		c->getDestTy()->print(o);
		o<<")";
#undef LEFT_BRACKET
#undef RIGHT_BRACKET
#define LEFT_BRACKET "("
#define RIGHT_BRACKET ")"
		PUSH_BRACKETS(14);
		lle::pretty_print(c->getOperand(0),o);
		POP_BRACKETS(14);
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
