#include "debug.h"
#include "SlashShrink.h"
#include <assert.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/CommandLine.h>
using namespace lle;
using namespace llvm;

cl::opt<bool> markd("Mark", cl::desc("Enable Mark some code on IR"));

StringRef MarkPreserve::MarkNode = "lle.mark";

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

void MarkPreserve::mark_all(Value* V)
{
   if(!V) return;
   Instruction* I = NULL;
   if(ConstantExpr* CE = dyn_cast<ConstantExpr>(V))
      I = CE->getAsInstruction();
   else I = dyn_cast<Instruction>(V);
   if(!I) return;

   mark(I);
   outs()<<__LINE__<<*I<<"\n";
   if(isa<LoadInst>(I))
      mark_all(I->getOperand(0));
   else
      assert(0);
}
