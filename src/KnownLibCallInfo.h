#ifndef KNOWN_LIBCALL_INFO_H_H
#define KNOWN_LIBCALL_INFO_H_H
#include <llvm/Analysis/LibCallSemantics.h>
namespace lle{
   /** provide the array of LibCallFunctionInfo
    * a environment variable LIBCALL_FILE can set the file to load
    * to build the array
    */
	class LibCallFromFile:public llvm::LibCallInfo
	{
      std::vector<llvm::LibCallFunctionInfo> Array;
		const llvm::LibCallFunctionInfo* getFunctionInfoArray() const {
         return Array.data();
      }

      public:
      LibCallFromFile();
      ~LibCallFromFile();
	};
}
#endif
