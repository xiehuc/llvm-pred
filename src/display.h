#ifndef LLE_DISPLAY_H_H
#define LLE_DISPLAY_H_H

#include <llvm/IR/Value.h>
#include <llvm/Support/raw_ostream.h>

namespace lle
{
	void pretty_print(llvm::Value* v,llvm::raw_ostream& o = llvm::outs());
	/** unfinished yet **/
	//void latex_print(llvm::Value* v);
}
#endif
