#ifndef _LOCK_INST_H_H
#define _LOCK_INST_H_H
#include <llvm/Pass.h>
#include <llvm/IR/Instruction.h>
/** a class used to lock the instructions
 * implementation: replace a inst with a call, which keeps all semantic 
 * useage: AU.addRequire<Lock>(); then use lock_inst method to lock
 * instructions.
 */
class Lock:public llvm::ModulePass
{
   public:
	static char ID;
	Lock():ModulePass(ID){}
	void getAnalysisUsage(llvm::AnalysisUsage& AU) const
	{
	    AU.setPreservesAll();
	}
	bool runOnModule(llvm::Module& M);
   llvm::Instruction* lock_inst(llvm::Instruction* I);
};

/* a class used to unlock instuctions
 * you shouldn't use this class directly. use opt instead:
 * opt -load ?.so -Unlock
 */
class Unlock:public llvm::ModulePass
{
   public:
	static char ID;
	Unlock():ModulePass(ID){}
	void getAnalysisUsage(llvm::AnalysisUsage& AU) const
	{
	    AU.setPreservesAll();
	}
	bool runOnModule(llvm::Module& M);
	void unlock_inst(llvm::Instruction* I);
};
#endif
