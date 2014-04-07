#ifndef LLE_LOOP_H_H
#define LLE_LOOP_H_H

#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Instructions.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Analysis/LoopPass.h>

#include <llvm/Support/CommandLine.h>

#include <iostream>
#include <map>
#include <stdlib.h>

extern llvm::cl::opt<bool> ValueProfiling;

namespace lle
{
	class LoopCycle:public llvm::LoopPass
	{
		std::map<llvm::Loop*,llvm::Value*> CycleMap;
		llvm::Loop* CurL;
		public:
		static char ID;
		explicit LoopCycle():LoopPass(ID){ }
		void getAnalysisUsage(llvm::AnalysisUsage&) const;
		bool runOnLoop(llvm::Loop* L,llvm::LPPassManager&);
		void print(llvm::raw_ostream&,const llvm::Module*) const;
		/**
		 * trying to find loop cycle, if it is a variable, because it is a
		 * sub instruction, first insert it into source then we can get it.
		 * if it is a constant, we couldn't simply insert a constant into
		 * source, so we directly return it. caller can make a cast
		 * instruction and insert it by hand.
		 */
		llvm::Value* insertLoopCycle(llvm::Loop* l);
		llvm::Value* getLoopCycle(llvm::Loop* l) const
		{ 
			return CycleMap.at(l);
		}
	};

	class LoopCycleSimplify:public llvm::LoopPass
	{
		llvm::Loop* CurL;
		public:
		static char ID;
		explicit LoopCycleSimplify():LoopPass(ID){}
		void getAnalysisUsage(llvm::AnalysisUsage&) const;
		bool runOnLoop(llvm::Loop* L,llvm::LPPassManager&);
		//bool runOnModule(llvm::Module&);
		void print(llvm::raw_ostream&,const llvm::Module*) const;
	};

}

#endif
