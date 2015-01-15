#ifndef LLVM_PRED_BLOCK_PROFILER_H_H
#define LLVM_PRED_BLOCK_PROFILER_H_H
#include <llvm/Pass.h>
#include <llvm/IR/Value.h>
#include <llvm/ADT/DenseMap.h>
#include <vector>

namespace llvm{
	class PredBlockProfiler: public ModulePass
	{
      static PredBlockProfiler* ins;

      //Block -> {step value, insert point}
      typedef llvm::DenseMap<llvm::BasicBlock*, std::pair<llvm::Value*, llvm::Value*> > TrapTy; 
		TrapTy BlockTraps;
		public:
		static char ID;
		PredBlockProfiler();
      //if opt enable -insert-pred-profiling
      static bool Avaliable(){ return ins != NULL;}
		/* 
       * insert a block increase counter in InsertTail
       * only if avaliable
       */
		static void increaseBlockCounter(BasicBlock* Block, Value* inc, BasicBlock* InsertTail) {
         if(ins != NULL) ins->BlockTraps[Block] = std::make_pair(inc, InsertTail);
      }
		static void increaseBlockCounter(BasicBlock* Block, Value* inc, Instruction* InsertBefore) {
         if(ins != NULL) ins->BlockTraps[Block] = std::make_pair(inc, InsertBefore);
      }
		//insert initial profiling for module
		bool runOnModule(Module& M);
		virtual const char* getPassName() const{
			return "BlockProfiler";
		}
	};
}
#endif
