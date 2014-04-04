#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/PassManager.h>
#include <llvm/Bitcode/ReaderWriter.h>

#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/ManagedStatic.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/system_error.h>
#include <llvm/Support/raw_ostream.h>

#include <llvm/IR/Instructions.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Transforms/Utils/LoopUtils.h>
#include <llvm/Analysis/MemoryDependenceAnalysis.h>

#include <ValueProfiling.h>

#include <fcntl.h>
#include <sys/stat.h>

#include "loop.h"
#include "util.h"
#include "debug.h"

using namespace std;
using namespace llvm;

namespace {
	cl::opt<std::string>
		BitcodeFile(cl::Positional, cl::desc("<program bitcode file>"),
				cl::Required);
	cl::opt<std::string>
		WriteFile(cl::Positional,cl::desc("<write bitcode file>"),
				cl::Optional);
	cl::opt<bool>
		ValueProfiling("insert-value-profiling", cl::desc("insert value profiling for loop cycle"));
	cl::opt<bool>
		PrettyPrint("pretty-print", cl::desc("pretty print loop cycle"));
};

static ValueProfiler* VProf = NULL;

class LoopPrintPass:public FunctionPass 
{
	static char ID;
	std::string delay_str;
	raw_string_ostream delay;
	unsigned unresolvedNum;
	const Function* curFunc;
	public:
	explicit LoopPrintPass(): 
		FunctionPass(ID),delay(delay_str)
	{
		unresolvedNum = 0;
	}
	void getAnalysisUsage(AnalysisUsage &AU) const 
	{
		//AU.setPreservesAll();
		AU.addRequired<LoopInfo>();
		AU.addRequired<AliasAnalysis>();
		AU.addRequired<MemoryDependenceAnalysis>();
	}
	void printUnresolved(raw_ostream& out)
	{
		if(unresolvedNum){
			outs()<<std::string(73,'*')<<"\n";
			outs()<<"\tNote!! there are "<<unresolvedNum<<" loop cycles unresolved:\n";
			outs()<<delay.str();
		}
	}
	void tryResolve(Value* V)
	{
		vector<Value*> resolved;
		vector<Instruction*> unsolved;
		unsolved = lle::resolve(V, resolved);
		outs()<<"::resolved list\n";
		for(auto i : resolved)
			outs()<<*i<<"\n";
		outs()<<"::unresolved\n";
		for(auto I : unsolved){
			SmallVector<lle::FindedDependenciesType,64> Result;
			outs()<<"::possible dependence for "<<*I<<"\n";
			lle::find_dependencies(I, this, Result);
			for(auto f : Result)
				outs()<<f.first<<"\t"<<*f.first.getInst()<<" in '"<<f.second->getName()<<"'\n";
		}
	}
	void runOnLoop(Loop* l)
	{
		lle::Loop L(l);
		if(L->getLoopPreheader()==NULL)
			InsertPreheaderForLoop(l, this);
		Value* CC = L.insertLoopCycle();
		if(CC){
			outs()<<*l<<"\n";
			outs()<<"cycles:";
			if(PrettyPrint){
				lle::pretty_print(L.getLoopCycle());
				outs()<<"\n";
				tryResolve(L.getLoopCycle());
			}else
				outs()<<*L.getLoopCycle();
			outs()<<"\n";
			if(ValueProfiling)
				VProf->insertValueTrap(CC, L->getLoopPreheader()->getTerminator());
		}else{
			++unresolvedNum;
			delay<<"Function:"<<curFunc->getName()<<"\n";
			delay<<"\t"<<*l<<"\n";
			//outs()<<"cycles:unknow\n";
		}
		if(!L->getSubLoops().empty()){
			for(auto I = L->getSubLoops().begin(),E = L->getSubLoops().end();I!=E;I++)
				runOnLoop(*I);
		}
	}
	bool runOnFunction(Function& F)
	{
		curFunc = &F;
		StringRef func_name = F.getName();
		outs()<<"------------------------\n";
		outs()<<"Function:"<<func_name<<"\n";
		outs()<<"------------------------\n";
		LoopInfo& LI = getAnalysis<LoopInfo>();
		for(auto ite = LI.begin(), end = LI.end();ite!=end;ite++){
			runOnLoop(*ite);
		}
		return true;
	}
};

char LoopPrintPass::ID = 0;

int main(int argc, char **argv) {
	// Print a stack trace if we signal out.
	sys::PrintStackTraceOnErrorSignal();
	PrettyStackTraceProgram X(argc, argv);

	LLVMContext &Context = getGlobalContext();
	llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.

	cl::ParseCommandLineOptions(argc, argv, "llvm profile dump decoder\n");

	// Read in the bitcode file...
	std::string ErrorMessage;
	OwningPtr<MemoryBuffer> Buffer;
	error_code ec;
	Module *M = 0;
	if (!(ec = MemoryBuffer::getFileOrSTDIN(BitcodeFile, Buffer))) {
		M = ParseBitcodeFile(Buffer.get(), Context, &ErrorMessage);
	} else
		ErrorMessage = ec.message();
	if (M == 0) {
		errs() << argv[0] << ": " << BitcodeFile << ": "
			<< ErrorMessage << "\n";
		return 1;
	}

	// Read the profiling information. This is redundant since we load it again
	// using the standard profile info provider pass, but for now this gives us
	// access to additional information not exposed via the ProfileInfo
	// interface.

	VProf = new ValueProfiler();
	LoopPrintPass* LPP = new LoopPrintPass();
	FunctionPassManager f_pass_mgr(M);
	f_pass_mgr.add(new LoopInfo());

	f_pass_mgr.add(createBasicAliasAnalysisPass());
	f_pass_mgr.add(createScalarEvolutionAliasAnalysisPass());
	f_pass_mgr.add(new MemoryDependenceAnalysis());

	f_pass_mgr.add(LPP);

	for( auto& func : *M ){
		f_pass_mgr.run(func);
	}
	if(ValueProfiling)
		VProf->runOnModule(*M);

	LPP->printUnresolved(outs());

	if(!WriteFile.empty()){
		std::string error;
		raw_fd_ostream output(WriteFile.c_str(),error);
		WriteBitcodeToFile(M, output);
	}
	return 0;
}
