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
#include <llvm/Transforms/Utils/SimplifyIndVar.h>

using namespace llvm;

namespace {
  cl::opt<std::string>
  BitcodeFile(cl::Positional, cl::desc("<program bitcode file>"),
              cl::Required);
};

namespace {
    class LoopPrintPass:public FunctionPass 
    {
        static char ID;
        public:
        explicit LoopPrintPass(): FunctionPass(ID) {}
        void getAnalysisUsage(AnalysisUsage &AU) const 
        {
            AU.setPreservesAll();
            AU.addRequired<LoopInfo>();
        }
        bool runOnFunction(Function& F)
        {
            outs()<<"Function:"<<F.getName()<<"\n";
            LoopInfo& LI = getAnalysis<LoopInfo>();
            LI.print(outs(), F.getParent());
			for(auto ite = LI.begin(), end = LI.end();ite!=end;ite++){
				Loop* L = *ite;
				outs()<<"phi node: ";
				PHINode* phi = L->getCanonicalInductionVariable();
				phi->print(outs());
				outs()<<"\n";
				if(phi == NULL) continue;
				outs()<<"--- users ---"<<"\n";
				Value* v1 = phi->getIncomingValue(0);
				for(auto use = v1->use_begin(),end = v1->use_end();use!=end;use++){
					if(isa<Instruction>(*use)){
						Instruction* i = dyn_cast<Instruction>(*use);
						// this is self node, so ignore it.
						if ( i == phi ) continue;
						i->print(outs());outs()<<"\n";
						if(i->getNumOperands() != 2) continue;
						assert((i->getNumOperands() == 2) && "if num operands isn't 2 , what it could be?");
						int idx = -1;
						idx = i->getOperand(0) == v1 ? 1:0;
						Use& bnd = i->getOperandUse(idx);
						//operand(idx) is another variable, which may be boundary condition;
						if(!bnd->getName().startswith("bnd.")) continue;
						outs()<<"boundary variable: "<<bnd->getName()<<"\n";
						for( auto bu = bnd->use_begin();bu != bnd->use_end();bu++){
							bu->print(outs());
							outs()<<"\n";
						}
					}else if(isa<Constant>(*use)){
					}else{
						assert(1&&"this is not a instruction, so what is it?");
					}
				}
				outs()<<"\n";
			}
            outs()<<"----------------------\n";

            return 0;
        }
    };
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

  FunctionPassManager f_pass_mgr(M);
  f_pass_mgr.add(new LoopInfo());
  f_pass_mgr.add(new LoopPrintPass());

  for( auto& func : *M ){
      f_pass_mgr.run(func);
  }


  return 0;
}
