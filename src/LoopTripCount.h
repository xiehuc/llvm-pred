#ifndef LLE_LOOP_H_H
#define LLE_LOOP_H_H

#include <llvm/ADT/DenseMap.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Support/raw_ostream.h>

#include <iostream>
#include <map>
#include <stdlib.h>

namespace lle
{
	class LoopTripCount:public llvm::FunctionPass
	{
		//statistics variable:{
		std::string unfound_str;
		llvm::raw_string_ostream unfound;
		unsigned NumUnfoundCycle;
		//}

		std::map<llvm::Loop*,llvm::Value*> CycleMap;
		public:
		static char ID;
		explicit LoopTripCount():FunctionPass(ID),unfound(unfound_str){
			NumUnfoundCycle = 0;
		}
		virtual ~LoopTripCount();
		void getAnalysisUsage(llvm::AnalysisUsage&) const;
		bool runOnFunction(llvm::Function& F);
		void print(llvm::raw_ostream&,const llvm::Module*) const;
		/**
		 * trying to find loop cycle, if it is a variable, because it is a
		 * sub instruction, first insert it into source then we can get it.
		 * if it is a constant, we couldn't simply insert a constant into
		 * source, so we directly return it. caller can make a cast
		 * instruction and insert it by hand.
		 */
		llvm::Value* insertTripCount(llvm::Loop* l, llvm::Instruction* InsertPos);
		llvm::Value* getTripCount(llvm::Loop* l);
      llvm::Value* getTripCount(llvm::Loop* l) const;
      // a helper function which convenient.
      llvm::Value* getOrInsertTripCount(llvm::Loop* l);
	};
}

#endif
