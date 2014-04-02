#include "KnownLibCallInfo.h"
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/LibCallAliasAnalysis.h>

using namespace lle;
using namespace llvm;

namespace llvm{
//void initializeKnownLCAAPass(PassRegistry&);
}

class KnownLCAA:public LibCallAliasAnalysis
{
	public:
	static char ID;
	explicit KnownLCAA() : LibCallAliasAnalysis(ID,NULL){
		this->LCI = new KnownLibCall();
//		initializeKnownLCAAPass(*PassRegistry::getPassRegistry());
	}
};

char KnownLCAA::ID = 0;
/*INITIALIZE_AG_PASS(KnownLCAA, AliasAnalysis, "klc-aa",
                   "KnownLibCall Alias Analysis", false, true, false)
						 */
static RegisterPass<KnownLCAA> X("klc-aa","KnowLibCall Alias Analysis",false,true);

const LibCallFunctionInfo* KnownLibCall::getFunctionInfoArray() const
{
	static LibCallFunctionInfo Array[] = {
		{"llvm.memcpy.p0i8.p0i8.i64",AliasAnalysis::ModRefResult::NoModRef}
	};
	return Array;
}
