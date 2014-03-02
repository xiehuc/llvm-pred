#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Instructions.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/Support/raw_ostream.h>

#include <iostream>
#include <stdlib.h>

#include <ValueProfiling.h>

namespace lle
{
	class Loop{
		llvm::Value* cycle;
		llvm::BasicBlock* insertBB;
		llvm::Loop* loop;
		Loop& self;
		// 为了能够直接转型(cast),使用体外储存,未来需要改为使用ValueMap
		public:
			Loop(llvm::Loop* l):self(*this){
				loop = l;
				cycle = insertBB = NULL;
			}
			llvm::Loop* operator->(){ return loop; }
			llvm::Value* insertLoopCycle(llvm::ValueProfiler* Prof);
			llvm::Value* getLoopCycle(){ return cycle; }
			llvm::Instruction* getLoopInsertPos(){return insertBB->getTerminator();}
	};

	void pretty_print(llvm::Value* v);
	/** unfinished yet **/
	void latex_print(llvm::Value* v);
}
