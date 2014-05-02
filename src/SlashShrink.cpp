#include "Resolver.h"
#include "SlashShrink.h"

#include <stdlib.h>
#include <unordered_set>

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Analysis/Verifier.h>
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

list<Value*> MarkPreserve::mark_all(Value* V, ResolverBase& R)
{
   list<Value*> empty;
   if(!V) return empty;
   Instruction* I = NULL;
   if(ConstantExpr* CE = dyn_cast<ConstantExpr>(V))
      I = CE->getAsInstruction();
   else I = dyn_cast<Instruction>(V);
   if(!I) return empty;

   mark(I);
   ResolveResult Res = R.resolve(I, [](Value* V){
         if(Instruction* Inst = dyn_cast<Instruction>(V))
            mark(Inst);
         });

   return get<1>(Res);
}

bool SlashShrink::runOnFunction(Function &F)
{
   int ShrinkLevel = atoi(getenv("SHRINK_LEVEL")?:"1");
   runtime_assert(ShrinkLevel>=0 && ShrinkLevel <=3);
   // mask all br inst to keep structure
   for(auto BB = F.begin(), E = F.end(); BB != E; ++BB){
      list<Value*> unsolved, left;
      unsolved = MarkPreserve::mark_all<UseOnlyResolve>(BB->getTerminator());
      for(auto I : unsolved){
         MarkPreserve::mark_all<NoResolve>(I);
      }
      for(auto I = BB->begin(), E = BB->end(); I != E; ++I){
         if(MarkPreserve::is_marked(I))
            MarkPreserve::mark_all<NoResolve>(I);

         if(StoreInst* SI = dyn_cast<StoreInst>(I)){
            Instruction* LHS = dyn_cast<Instruction>(SI->getOperand(0));
            Instruction* RHS = dyn_cast<Instruction>(SI->getOperand(1));
            if(LHS && RHS && MarkPreserve::is_marked(LHS) && MarkPreserve::is_marked(RHS))
               MarkPreserve::mark(SI);
         }

         if(CallInst* CI = dyn_cast<CallInst>(I)){
            Function* Func = CI->getCalledFunction();
            if(!Func) continue;
            if(Func->empty()) continue; /* a func's body is empty, means it is
                                           not a native function */

            MarkPreserve::mark_all<NoResolve>(CI);
         }

      }
   }

   if(ShrinkLevel == 0) return false;

   for(auto BB = F.begin(), E = F.end(); BB != E; ++BB){
      auto I = BB->begin();
      while(I != BB->end()){
         if(!MarkPreserve::is_marked(I)){
            for(uint i=0;i<I->getNumOperands();++i)
               I->setOperand(i, NULL); /* destroy instruction need clean holds
                                          reference */
            (I++)->removeFromParent(); /* use erase from would cause crash let
                                          it freed by Context */
         }else
            ++I;
      }
   }

   return true;
}
