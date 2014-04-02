#ifndef KNOWN_LIBCALL_INFO_H_H
#define KNOWN_LIBCALL_INFO_H_H
#include <llvm/Analysis/LibCallSemantics.h>
namespace lle{
	class KnownLibCall:public llvm::LibCallInfo
	{
		virtual const llvm::LibCallFunctionInfo* getFunctionInfoArray() const;
	};
}
#endif
