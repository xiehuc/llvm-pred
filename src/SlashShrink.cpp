#include "Resolver.h"
#include "SlashShrink.h"

#include <unordered_set>

#include <llvm/IR/Function.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/CommandLine.h>

#include "debug.h"

using namespace std;
using namespace lle;
using namespace llvm;

cl::opt<bool> markd("Mark", cl::desc("Enable Mark some code on IR"));

StringRef MarkPreserve::MarkNode = "lle.mark";

char SlashShrink::ID = 0;

static RegisterPass<SlashShrink> X("Shrink", "Slash and Shrink Code to make a minicore program");

bool MarkPreserve::enabled()
{
   return ::markd;
}

void MarkPreserve::mark(Instruction* Inst)
{
   if(!Inst) return;
   if(is_marked(Inst)) return;

   LLVMContext& C = Inst->getContext();
   MDNode* N = MDNode::get(C, MDString::get(C, "mark"));
   Inst->setMetadata(MarkNode, N);
}

void MarkPreserve::mark_all(Value* V, ResolverBase& R)
{
   if(!V) return;
   Instruction* I = NULL;
   if(ConstantExpr* CE = dyn_cast<ConstantExpr>(V))
      I = CE->getAsInstruction();
   else I = dyn_cast<Instruction>(V);
   if(!I) return;

   mark(I);
   R.resolve(I, [](Value* V){
         if(Instruction* Inst = dyn_cast<Instruction>(V))
            mark(Inst);
         });
}

bool SlashShrink::runOnFunction(Function &F)
{
   for(auto BB = F.begin(), E = F.end(); BB != E; ++BB){
      MarkPreserve::mark_all<UseOnlyResolve>(BB->getTerminator());
   }

   return false;
}
