#ifndef LLE_LOOP_H_H
#define LLE_LOOP_H_H

#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Instructions.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Analysis/LoopPass.h>

#include <iostream>
#include <map>
#include <stdlib.h>

namespace lle
{
	class LoopCycle:public llvm::LoopPass
	{
		std::map<llvm::Loop*,llvm::Value*> CycleMap;
		public:
		static char ID;
		explicit LoopCycle():LoopPass(ID){ }
		void getAnalysisUsage(llvm::AnalysisUsage&) const;
		virtual bool runOnLoop(llvm::Loop*,llvm::LPPassManager&){return false;}
		/**
		 * trying to find loop cycle, if it is a variable, because it is a
		 * sub instruction, first insert it into source then we can get it.
		 * if it is a constant, we couldn't simply insert a constant into
		 * source, so we directly return it. caller can make a cast
		 * instruction and insert it by hand.
		 */
		llvm::Value* insertLoopCycle(llvm::Loop* l);
		llvm::Value* getLoopCycle(llvm::Loop* l)
		{ 
			if(CycleMap[l]==NULL)
				insertLoopCycle(l);
			return CycleMap[l]; 
		}
	};

}

#endif
