#include "util.h"
#include "debug.h"

#include <set>
#include <string>
#include <algorithm>

#include <llvm/IR/Function.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/GlobalVariable.h>


using namespace std;
using namespace lle;
using namespace llvm;

//phinode circle recursion visit magic word
#define PHINODE_CIRCLE "Δ"
#define LEFT_BRACKET "{"
#define RIGHT_BRACKET "}"
#define PUSH_BRACKETS(precd) if(precd>op_precd.back()) o<<LEFT_BRACKET; op_precd.push_back(precd);
#define POP_BRACKETS(precd) op_precd.pop_back(); if(precd>op_precd.back()) o<<RIGHT_BRACKET;

static SmallVector<int,16> op_precd((unsigned)1,16); // operator precedence with 1 element 16 back

const pair<const char*, int>& lle::lookup_sym(BinaryOperator* BO)
{
	static const std::map<int,std::pair<const char*,int> > symbols = {
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

   return symbols.at(BO->getOpcode());
}

static void pretty_print(BinaryOperator* bin,raw_ostream& o)
{
	int precd = lookup_sym(bin).second;
	PUSH_BRACKETS(precd);

	pretty_print(bin->getOperand(0),o);
   Assert(lookup_sym(bin).first, "unknow operator");
	//if operand 1 is negative ,should ignore + symbol (+-1)-> (-1)
	bool ignore = false;
	if(bin->getOpcode()==Instruction::Add){
		ConstantInt* CInt = dyn_cast<ConstantInt>(bin->getOperand(1));
		if(CInt && CInt->isNegative()) ignore = true;
		ConstantFP* CFP = dyn_cast<ConstantFP>(bin->getOperand(1));
		if(CFP && CFP->isNegative()) ignore = true;
	}
	if(!ignore)
		o<<" "<<lookup_sym(bin).first<<" ";
	pretty_print(bin->getOperand(1),o);

	POP_BRACKETS(precd);
}

const pair<const char*, int>& lle::lookup_sym(CmpInst* CI)
{
	static const std::map<int,std::pair<const char*,int> > symbols = {
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

   return symbols.at(CI->getPredicate());
}

static void pretty_print(CmpInst* cmp,raw_ostream& o)
{
	int precd = lookup_sym(cmp).second;
	PUSH_BRACKETS(precd);

	pretty_print(cmp->getOperand(0),o);
	o<<" "<<lookup_sym(cmp).first<<" ";
	pretty_print(cmp->getOperand(1),o);

	POP_BRACKETS(precd);
}

static void pretty_print(Constant* c,raw_ostream& o)
{
	if(auto CI = dyn_cast<ConstantInt>(c))
		CI->getValue().print(o, true);
	else if(auto CFP = dyn_cast<ConstantFP>(c))
		o<< CFP->getValueAPF().convertToDouble();
	else if(isa<GlobalValue>(c))
		o<<"@"<<c->getName();
	else if(auto CE = dyn_cast<ConstantExpr>(c))
		lle::pretty_print(CE->getAsInstruction(),o);
	else
		o<<*c;
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
		if((Rstr.compare(0, 3, PHINODE_CIRCLE"#")==0)&&std::all_of(Rstr.begin()+3,Rstr.end(),::isdigit)) continue;
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

static void pretty_print(LoadInst* LI, raw_ostream& O, bool E)
{
   O<<"*";
   if(E) lle::pretty_print(LI->getOperand(0),O);
   else{
      string S;
      raw_string_ostream SS(S);
      LI->getOperand(0)->print(SS);
      int beg = SS.str().find_first_not_of(" ");
      int end = SS.str().find(" =");
      S = SS.str().substr(beg,end-beg);
      O<<S;
   }
}

void lle::pretty_print(Value* v,raw_ostream& o, bool expand)
{
	if(auto C = dyn_cast<Constant>(v)){
		::pretty_print(C,o);
		return;
	}
	if(isa<Argument>(v))
		o<<"%"<<v->getName();
	Instruction* inst = dyn_cast<Instruction>(v);
	if(!inst) return;

	if(inst->isBinaryOp())
		::pretty_print(cast<BinaryOperator>(inst),o);
	else if(auto CI = dyn_cast<CmpInst>(inst))
		::pretty_print(CI,o);
	else if(auto P = dyn_cast<PHINode>(inst))
		::pretty_print(P,o);
	else if(auto LI = dyn_cast<LoadInst>(inst)){
      ::pretty_print(LI, o, expand);
	}else if(isa<AllocaInst>(inst))
		o<<"%"<<inst->getName();
	else if(isa<SelectInst>(inst)){
		Assert(inst->getNumOperands()==3, *inst<<"select should have only 3 operant");
		PUSH_BRACKETS(14);
		o<<"(";
		lle::pretty_print(inst->getOperand(0),o);
		o<<")?";
		lle::pretty_print(inst->getOperand(1),o);
		o<<":";
		lle::pretty_print(inst->getOperand(2),o);
		POP_BRACKETS(14);
	}
	else if(auto CI = dyn_cast<CastInst>(inst)){
		o<<"(";
		CI->getDestTy()->print(o);
		o<<")";
#undef LEFT_BRACKET
#undef RIGHT_BRACKET
#define LEFT_BRACKET "("
#define RIGHT_BRACKET ")"
		PUSH_BRACKETS(14);
		lle::pretty_print(CI->getOperand(0),o);
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
		Assert(0,*inst<<"not defined instruction print");
	}
}


Value* lle::castoff(Value* v)
{
   if(ConstantExpr* CE = dyn_cast<ConstantExpr>(v))
      v = CE->getAsInstruction();
	if(CastInst* CI = dyn_cast<CastInst>(v)){
		return castoff(CI->getOperand(0));
	}else
		return v;
}

Argument* lle::findCallInstArgument(Use* use)
{
   CallInst* CI = dyn_cast<CallInst>(use->getUser());
   Value* v = use->get();
   if(!CI||!v) return NULL;
   uint idx = find(CI->op_begin(), CI->op_end(), 
         v) - CI->op_begin(); //find argument position FIXME 这里有问题
   Assert(idx != CI->getNumOperands(), "");
   Function* CF = dyn_cast<Function>(castoff(CI->getCalledValue()));
   Assert(CF," called function should not be null");
   if(CF->getArgumentList().size() <= idx) // there are no enough argument
      // FIXME: 通常，调用的外部函数是(...)格式，也就是说不知道会不会写入！
      return NULL;
   auto ite = CF->getArgumentList().begin();
   advance(ite,idx); /* get function argument */
   return &*ite;
}
