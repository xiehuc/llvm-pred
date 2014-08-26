#ifndef LLE_DISPLAY_H_H
#define LLE_DISPLAY_H_H

#include <list>
#include <vector>
#include <llvm/IR/Value.h>
#include <llvm/IR/Argument.h>
#include <llvm/IR/Instructions.h>
#include <llvm/ADT/GraphTraits.h>
#include <llvm/Support/raw_ostream.h>

#define WALK_THROUGH_DEPTH 10

namespace lle
{
   // find string expr and priority for CmpInst and BinaryOperator
   const std::pair<const char*, int>& lookup_sym(llvm::CmpInst* CI);
   const std::pair<const char*, int>& lookup_sym(llvm::BinaryOperator* BO);

   // @param expand : true  --> nest expand the load inst's target
   //                 false --> just print the name for load inst's target
	void pretty_print(llvm::Value* v,llvm::raw_ostream& o = llvm::outs(), bool expand = true);

   // check a Argument is write to memory
   // Argument must be PointerType
   // if Function has lle.arg.write attribute. it would consider it first.
   // or it would check argument use to find whether have a store instruction.
   bool isArgumentWrite(llvm::Argument* Arg);

   bool isArray(llvm::Value*);

	//remove cast instruction for a value
	//because cast means the original value and the returned value is
	//semanticly equal
	llvm::Value* castoff(llvm::Value* v);
   //from a given callinst's use, find the function argument
   llvm::Argument* findCallInstArgument(llvm::Use* use);
   // from a given function's argument and callinst, return the use
   inline llvm::Use* findCallInstParameter(llvm::Argument* Arg, llvm::CallInst* CI){
      return &CI->getOperandUse(Arg->getArgNo());
   }
   //find where to use V on instuction I
   inline llvm::Use* findOpUseOnInstruction(llvm::Instruction* I, llvm::Value* V){
      for(auto O = I->op_begin(), E = I->op_end(); O!=E; ++O){
         if(O->get() == V) return &*O;
      }
      return nullptr;
   }

}

#endif
