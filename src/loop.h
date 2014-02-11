#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Instructions.h>

#include <iostream>
#include <llvm/Support/raw_ostream.h>

namespace ll 
{

	using namespace llvm;

	namespace Loop{
		Value* getInductionStartValue(llvm::Loop* loop);
		Value* getCanonicalEndCondition(llvm::Loop* loop);
	}

	void pretty_print(Value* v);
	/** unfinished yet **/
	void latex_print(Value* v);
}
