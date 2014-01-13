#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Instructions.h>

#include <iostream>
#include <llvm/Support/raw_ostream.h>
using namespace std;

namespace ll {
	class Loop{
		public:
			static llvm::Value* 
				getCanonicalEndCondition(llvm::Loop* loop)
				{
					//i
					llvm::PHINode* ind = loop->getCanonicalInductionVariable();
					if(ind == NULL) return NULL;
					//i++
					llvm::Value* indpp = ind->getIncomingValueForBlock(loop->getLoopLatch());
					for(auto ii = indpp->use_begin(), ee = indpp->use_end();ii!=ee;++ii){
						llvm::Instruction* inst = llvm::dyn_cast<llvm::Instruction>(*ii);
						if(!inst) continue;
						if(inst->getOpcode() == llvm::Instruction::ICmp){
							llvm::Value* v1 = inst->getOperand(0);
							llvm::Value* v2 = inst->getOperand(1);
							if(v1 == indpp && loop->isLoopInvariant(v2)) return v2;
							if(v2 == indpp && loop->isLoopInvariant(v1)) return v1;
						}
					}
					return NULL;
				}
	};

	void pretty_print(llvm::Value* v)
	{
		v->print(llvm::outs());llvm::outs()<<"\n";
		llvm::Instruction* inst = llvm::dyn_cast<llvm::Instruction>(v);
		if(!inst) return;
		if(inst->mayReadOrWriteMemory()) return;
		for(auto opi = inst->op_begin(),ope = inst->op_end(); opi!=ope; ++opi){
			pretty_print(*opi);
		}
	}
}
