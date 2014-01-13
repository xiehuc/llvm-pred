#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Instructions.h>

#include <iostream>
#include <llvm/Support/raw_ostream.h>

namespace ll {

	using namespace llvm;

	class Loop{
		public:
			static Value* 
				getCanonicalEndCondition(llvm::Loop* loop)
				{
					//i
					PHINode* ind = loop->getCanonicalInductionVariable();
					if(ind == NULL) return NULL;
					//i++
					Value* indpp = ind->getIncomingValueForBlock(loop->getLoopLatch());
					for(auto ii = indpp->use_begin(), ee = indpp->use_end();ii!=ee;++ii){
						Instruction* inst = dyn_cast<Instruction>(*ii);
						if(!inst) continue;
						if(inst->getOpcode() == Instruction::ICmp){
							Value* v1 = inst->getOperand(0);
							Value* v2 = inst->getOperand(1);
							if(v1 == indpp && loop->isLoopInvariant(v2)) return v2;
							if(v2 == indpp && loop->isLoopInvariant(v1)) return v1;
						}
					}
					return NULL;
				}
	};

	void pretty_print(BinaryOperator* bin);
	void pretty_print(Value* v);
}
