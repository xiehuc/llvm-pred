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
		//initializeKnownLCAAPass(*PassRegistry::getPassRegistry());
	}
	virtual bool runOnFunction(Function& F)
	{
		InitializeAliasAnalysis(this);
		return false;
	}
	//print nothing
	void print(raw_ostream& OS, const Module*) const
	{
	}
};

char KnownLCAA::ID = 0;
// 3.4 的doc里面写的用下面的宏来注册，但是 `2.2`_ 写的是用更下面的模板来注册,两
//   者关键的区别是用宏来注册需要手工调用一次, 而模板的不需要,适合opt -load 这种情况.
//
// .. _2.2: http://www.cs.ucla.edu/classes/spring08/cs259/llvm-2.2/docs/WritingAnLLVMPass.html#analysisgroup
//
// INITIALIZE_AG_PASS(KnownLCAA, AliasAnalysis, "klc-aa",
//                   "KnownLibCall Alias Analysis", false, true, false)
// 注册自身为Pass
static RegisterPass<KnownLCAA> X("klc-aa","KnowLibCall Alias Analysis",false,true);
// 将Pass加入到AliasAnalysis中.
// 3.4 的doc中写道RegisterAnalysisGroup是注册AG本身(构造参数为char*),但是明显这
//   里的语义是将X加入到AA中(构造参数为PassInfo),这两个语义区别太大,目前只能估
//   计是靠构造函数来区分两种语义的,也不知道下面的用法是不是不被认可了.
static RegisterAnalysisGroup<AliasAnalysis> Y(X);

const LibCallFunctionInfo* KnownLibCall::getFunctionInfoArray() const
{
	static LibCallFunctionInfo Array[] = {
		{"llvm.memcpy.p0i8.p0i8.i64" , AliasAnalysis::ModRefResult::NoModRef} , 
		{"llvm.lifetime.end"         , AliasAnalysis::ModRefResult::NoModRef} , 
		{"_gfortran_st_write_done"   , AliasAnalysis::ModRefResult::NoModRef} , 
		{"_gfortran_st_write"        , AliasAnalysis::ModRefResult::NoModRef} , 
		{"free"                      , AliasAnalysis::ModRefResult::NoModRef} , 
	};
	return Array;
}
