#ifndef LLE_DISPLAY_H_H
#define LLE_DISPLAY_H_H

#include <llvm/IR/Value.h>

namespace lle
{
	void pretty_print(llvm::Value* v);
	/** unfinished yet **/
	//void latex_print(llvm::Value* v);
}
#endif
