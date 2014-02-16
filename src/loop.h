#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Instructions.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/Support/raw_ostream.h>

#include <iostream>
#include <stdlib.h>

namespace lle
{
	class Loop:public llvm::Loop{
		struct Storage{
			llvm::Value* cycle;
		};
		// 为了能够直接转型(cast),使用体外储存,未来需要改为使用ValueMap
		static llvm::DenseMap<llvm::Loop*,Storage> stores;
		public:
			llvm::Value* getInductionStartValue();
			llvm::Value* getCanonicalEndCondition();
			llvm::Value* insertLoopCycle();
			llvm::Value* getLoopCycle(){
				return (stores[this].cycle)?:insertLoopCycle();
			}
	};

	void pretty_print(llvm::Value* v);
	/** unfinished yet **/
	void latex_print(llvm::Value* v);
}
