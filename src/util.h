#ifndef LLE_DISPLAY_H_H
#define LLE_DISPLAY_H_H

#include <list>
#include <vector>
#include <llvm/IR/Value.h>
#include <llvm/IR/Instructions.h>
#include <llvm/ADT/GraphTraits.h>
#include <llvm/Support/raw_ostream.h>

#define WALK_THROUGH_DEPTH 10

namespace lle
{
   const std::pair<const char*, int>& lookup_sym(llvm::CmpInst* CI);
   const std::pair<const char*, int>& lookup_sym(llvm::BinaryOperator* BO);
	void pretty_print(llvm::Value* v,llvm::raw_ostream& o = llvm::outs());

	//remove cast instruction for a value
	//because cast means the original value and the returned value is
	//semanticly equal
	llvm::Value* castoff(llvm::Value* v);
   llvm::Argument* findCallInstArgument(llvm::Use* use);
}

#endif
