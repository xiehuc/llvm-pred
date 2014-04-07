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
};

static ValueProfiler* VProf = NULL;
#if 0
class LoopPrintPass:public LoopPass
{
	std::string delay_str;
	raw_string_ostream delay;
	unsigned unresolvedNum;
	const Function* curFunc;
	public:
	static char ID;
	explicit LoopPrintPass(): 
		LoopPass(ID),delay(delay_str)
	{
		unresolvedNum = 0;
	}
	void getAnalysisUsage(AnalysisUsage &AU) const 
	{
		AU.setPreservesAll();
		AU.addRequired<lle::LoopCycle>();
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
	bool runOnLoop(Loop* L,LPPassManager& LPM)
	{
	}
};

char LoopPrintPass::ID = 0;
static RegisterPass<LoopPrintPass> X("print-loop-cycle","Print Loop Cycle Pass",false,false);
#endif

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
	lle::LoopCycleSimplify* LPP = new lle::LoopCycleSimplify();
	PassManager pass_mgr;
	pass_mgr.add(createBasicAliasAnalysisPass());
	pass_mgr.add(createGlobalsModRefPass());
	pass_mgr.add(createScalarEvolutionAliasAnalysisPass());
	pass_mgr.add(new MemoryDependenceAnalysis());
	//pass_mgr.add(new LoopInfo());
	pass_mgr.add(LPP);

	if(ValueProfiling)
		pass_mgr.add(VProf);

	pass_mgr.run(*M);

	//LPP->printUnresolved(outs());

	if(!WriteFile.empty()){
		std::string error;
		raw_fd_ostream output(WriteFile.c_str(),error);
		WriteBitcodeToFile(M, output);
	}
	return 0;
}
