#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Instructions.h>

#include <iostream>
#include <llvm/Support/raw_ostream.h>

namespace lle
{
	class Loop:public llvm::Loop{
		public:
			llvm::Value* getInductionStartValue();
			llvm::Value* getCanonicalEndCondition();
	};

	void pretty_print(llvm::Value* v);
	/** unfinished yet **/
	void latex_print(llvm::Value* v);
}
