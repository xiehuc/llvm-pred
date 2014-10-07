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

		// a stable cache, the index of Loop in df order -> LoopTripCount
      std::vector<llvm::Value*> CycleMap;
      // an unstable cache, the Loop -> index of Loop in df order
      llvm::DenseMap<llvm::Loop*, size_t> LoopMap;
		/**
		 * trying to find loop cycle, if it is a variable, because it is a
		 * sub instruction, first insert it into source then we can get it.
		 * if it is a constant, we couldn't simply insert a constant into
		 * source, so we directly return it. caller can make a cast
		 * instruction and insert it by hand.
		 */
		llvm::Value* insertTripCount(llvm::Loop* l, llvm::Instruction* InsertPos);
		public:
		static char ID;
		explicit LoopTripCount():FunctionPass(ID),unfound(unfound_str){
			NumUnfoundCycle = 0;
		}
		virtual ~LoopTripCount();
		void getAnalysisUsage(llvm::AnalysisUsage&) const;
		bool runOnFunction(llvm::Function& F);
		void print(llvm::raw_ostream&,const llvm::Module*) const;
      // use this before getTripCount, to make a stable Loop Index Order
      void updateCache(llvm::LoopInfo& LI);
      /**trying to find inserted loop trip count in preheader 
       * don't use cache because Loop structure is not stable*/
      llvm::Value* getTripCount(llvm::Loop* l) const;
      // a helper function which convenient.
      llvm::Value* getOrInsertTripCount(llvm::Loop* l);
	};
}

#endif
