#include "preheader.h"

#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/Support/CommandLine.h>

#include <ProfileInfo.h>
#include <ProfileInfoWriter.h>
#include <vector>

#include "debug.h"

namespace lle{
   class ValueToEdgeProfiling: public llvm::ModulePass
   {
      llvm::DenseMap<const llvm::BasicBlock*, int> BlockExecutions;
      public:
      static char ID;
      ValueToEdgeProfiling():ModulePass(ID) {}
      void getAnalysisUsage(llvm::AnalysisUsage&) const override;
      bool runOnModule(llvm::Module&) override;
   };

}

using namespace lle;
using namespace llvm;

char ValueToEdgeProfiling::ID = 0;
static RegisterPass<ValueToEdgeProfiling> X("Value-To-Edge", "Convert "
      "Performance Predication Block Frequency Value Profiling to Edge Profiling "
      "Form", true, true);

static cl::opt<std::string> 
ProfileInfoOutputName("profile-info-output", cl::init("llvmprof.edge"),
      cl::value_desc("filename"), cl::desc("Profile file output by"
         "-Value-To-Edge"));

const BasicBlock* find_block_by_name(const Function* F, StringRef Name)
{
   auto Ite = find_if(F->begin(), F->end(), [Name](const BasicBlock& BB){
         return BB.getName() == Name;
         });
   return Ite == F->end()?NULL:&*Ite;
}

void ValueToEdgeProfiling::getAnalysisUsage(AnalysisUsage &AU) const
{
   AU.addRequired<ProfileInfo>();
   AU.setPreservesAll();
}

bool ValueToEdgeProfiling::runOnModule(Module & M)
{
   ProfileInfo& PI = getAnalysis<ProfileInfo>();
   for(const Instruction* I : PI.getAllTrapedValues()){
      const CallInst* CI = dyn_cast<CallInst>(I);
      if(CI == NULL) continue;
      int value = std::accumulate(PI.getValueContents(CI).begin(), PI.getValueContents(CI).end(), 0);
      const Value* Key = CI->getArgOperand(1);
      Assert(Key,"");
      StringRef Name = Key->getName();
      Name = Name.substr(0,Name.rfind(".bfreq"));
      const Function* F = I->getParent()->getParent();
      const BasicBlock* BB = find_block_by_name(F, Name);
      Assert(BB,"");
      errs()<<F->getName()<<":"<<"\n";
      errs()<<*CI<<"\n";
      errs()<<BB->getName()<<"\n";
      BlockExecutions[BB] = value;
   }


   std::vector<unsigned> Counters;
   if (Counters.size() > 0) {
      for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
         if (F->isDeclaration()) continue;
         for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
            Counters.push_back(BlockExecutions[BB]);
      }
   }

   ProfileInfoWriter PIWriter("", ProfileInfoOutputName);
   PIWriter.write(BlockInfo, Counters);

   return false;
}
