#ifndef LLE_LOOP_H_H
#define LLE_LOOP_H_H

#include <llvm/ADT/DenseMap.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Analysis/ScalarEvolution.h>

#include <iostream>
#include <map>
#include <stdlib.h>

namespace lle
{
   class NotFound: public std::runtime_error
   {
      size_t line_no;
      public:
      explicit NotFound(size_t src_line, const std::string& what):
         runtime_error(what), line_no(src_line) {}
      explicit NotFound(size_t src_line, llvm::raw_ostream& what):
         runtime_error(static_cast<llvm::raw_string_ostream&>(what).str()), line_no(src_line) {}
      size_t get_line(){ return line_no;}
   };
#define not_found(what) NotFound(__LINE__, what)

	class LoopTripCount:public llvm::FunctionPass
	{
		//statistics variable:{
		std::string unfound_str;
		llvm::raw_string_ostream unfound;
		//}
      llvm::LoopInfo* LI;
      struct AnalysisedLoop {
         int AdjustStep;
         llvm::Value* Start, *Step, *End, *Ind, *TripCount;
      };
      struct SCEV_Analysised{
         const llvm::SCEV* LoopInfo;
         llvm::Value* TripCount;
      };
		// a stable cache, the index of Loop in df order -> LoopTripCount
      std::vector<AnalysisedLoop> CycleMap;
      ///////////////////////////////////
      std::vector<SCEV_Analysised> SCEV_CycleMap;
      void SCEV_analysis(llvm::Loop*);
      // an unstable cache, the Loop -> index of Loop in df order
      llvm::DenseMap<llvm::Loop*, size_t> LoopMap;
      AnalysisedLoop analysis(llvm::Loop*);
		/**
		 * trying to find loop cycle, if it is a variable, because it is a
		 * sub instruction, first insert it into source then we can get it.
		 * if it is a constant, we couldn't simply insert a constant into
		 * source, so we directly return it. caller can make a cast
		 * instruction and insert it by hand.
		 */
      llvm::Value* SCEV_insertTripCount(const llvm::SCEV *scev_expr, llvm::StringRef, llvm::Instruction* InsertPos);
      llvm::Value* insertTripCount(AnalysisedLoop,llvm::StringRef, llvm::Instruction* InsertPos);
		public:
		static char ID;
		explicit LoopTripCount():FunctionPass(ID),unfound(unfound_str){ }
		void getAnalysisUsage(llvm::AnalysisUsage&) const;
		bool runOnFunction(llvm::Function& F);
		void print(llvm::raw_ostream&,const llvm::Module*) const;
      // use this before getTripCount, to make a stable Loop Index Order
      void updateCache(llvm::LoopInfo& LI);
      llvm::Loop* getLoopFor(llvm::BasicBlock* BB) const;
      llvm::Value* getTripCount(llvm::Loop* L) const {
         auto ite = LoopMap.find(L);
         return ite==LoopMap.end()?NULL:CycleMap[ite->second].TripCount;
      }
      ///////
      llvm::Value* SCEV_getTripCount(llvm::Loop* L) const{
         auto ite = LoopMap.find(L);
         return ite == LoopMap.end()?NULL:SCEV_CycleMap[ite->second].TripCount;
      }
      llvm::Value* getInduction(llvm::Loop* L) const {
         auto ite = LoopMap.find(L);
         return (ite==LoopMap.end())?NULL:CycleMap[ite->second].TripCount;
      }
      // a helper function which convenient.
      llvm::Value* getOrInsertTripCount(llvm::Loop* l);
	};
}

#endif
