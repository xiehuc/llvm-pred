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
		//statistics variable:{
		std::string unfound_str;
		llvm::raw_string_ostream unfound;
		unsigned NumUnfoundCycle;
		//}

		std::map<llvm::Loop*,llvm::Value*> CycleMap;
		llvm::Loop* CurL;
      llvm::Pass* ParentPass;
		public:
		static char ID;
		explicit LoopCycle():LoopPass(ID),unfound(unfound_str){
			NumUnfoundCycle = 0;
         ParentPass = NULL;
		}
      /** use this constructor when use it on in a FunctionPass
       * example in runOnFunction():
       *  LoopCycle LC(this); //this is a Function Pass
       *  LC.getOrInsertCycle(...);
       */
      explicit LoopCycle(Pass* parent):LoopCycle(){
         ParentPass = parent;
      }
		virtual ~LoopCycle();
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
			auto ite = CycleMap.find(l);
			return ite!=CycleMap.end()?ite->second:NULL;
		}
      // a helper function which convenient.
      llvm::Value* getOrInsertCycle(llvm::Loop* l)
      {
         return getLoopCycle(l)?:insertLoopCycle(l);
      }
	};
}

#endif
