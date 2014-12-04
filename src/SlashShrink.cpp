#include "preheader.h"
#include "Resolver.h"
#include "SlashShrink.h"
#include "KnownLibCallInfo.h"

#include <fstream>
#include <stdlib.h>
#include <unordered_set>

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Analysis/LibCallAliasAnalysis.h>

#include <ValueProfiling.h>

#include "util.h"
#include "debug.h"

using namespace std;
using namespace lle;
using namespace llvm;

cl::opt<bool> markd("Mark", cl::desc("Enable Mark some code on IR"));
cl::opt<bool> ExecuteTrap("ExecutePath-Trap", cl::desc("Trap Execute Path to"
         "get know how IR Executed, Implement based on value-profiling"));/*
         may be duplicated with EdgeProfiling's implement effect, need be
         delete*/

StringRef MarkPreserve::MarkNode = "lle.mark";

char SlashShrink::ID = 0;

static RegisterPass<SlashShrink> X("Shrink", "Slash and Shrink Code to make a minicore program");

bool MarkPreserve::enabled()
{
   return ::markd;
}

void MarkPreserve::mark(Instruction* Inst, StringRef origin)
{
   if(!Inst) return;
   if(is_marked(Inst)) return;

   LLVMContext& C = Inst->getContext();
   MDNode* N = MDNode::get(C, MDString::get(C, origin));
   Inst->setMetadata(MarkNode, N);
}

list<Value*> MarkPreserve::mark_all(Value* V, ResolverBase& R, StringRef origin)
{
   list<Value*> empty;
   if(!V) return empty;
   Instruction* I = NULL;
   if(ConstantExpr* CE = dyn_cast<ConstantExpr>(V))
      I = CE->getAsInstruction();
   else I = dyn_cast<Instruction>(V);
   if(!I) return empty;

   mark(I, origin);
   ResolveResult Res = R.resolve(I, [origin](Value* V){
         if(Instruction* Inst = dyn_cast<Instruction>(V))
            mark(Inst, origin);
         });

   return get<1>(Res);
}

SlashShrink::SlashShrink():FunctionPass(ID)
{
   const char* filepath = getenv("IGNOREFUNC_FILE");
   if(filepath){
      ifstream F(filepath);
      if(!F.is_open()){
         perror("Unable Open IgnoreFunc File");
         exit(-1);
      }

      string word;
      while(F>>word){
         IgnoreFunc.insert(word);
      }

      F.close();
   }

   ShrinkLevel = atoi(getenv("SHRINK_LEVEL")?:"1");
   AssertRuntime(ShrinkLevel>=0 && ShrinkLevel <=3, "");

}

void SlashShrink::getAnalysisUsage(AnalysisUsage& AU) const
{
   AU.addRequired<LibCallAliasAnalysis>();
   AU.addRequired<ResolverPass>();
}

bool SlashShrink::runOnFunction(Function &F)
{
   LibCallAliasAnalysis& LibCall = getAnalysis<LibCallAliasAnalysis>();
   ResolverPass& RP = getAnalysis<ResolverPass>();
   if(LibCall.LCI == NULL) LibCall.LCI = new LibCallFromFile(); /* use LibCall
                                                               to cache LCI */
   LLVMContext& C = F.getContext();
   Constant* Zero = ConstantInt::get(Type::getInt32Ty(C), 0);
   // mask all br inst to keep structure
   for(auto BB = F.begin(), E = F.end(); BB != E; ++BB){
      list<Value*> unsolved, left;
      if(ExecuteTrap){
         Value* Prof = ValueProfiler::insertValueTrap(
               Zero, BB->getTerminator());
         MarkPreserve::mark(dyn_cast<Instruction>(Prof));
      }

      unsolved = MarkPreserve::mark_all(BB->getTerminator(), RP.getResolver<UseOnlyResolve>(), "terminal");
      for(auto I : unsolved){
         MarkPreserve::mark_all(I, RP.getResolver<NoResolve>(), "terminal");
      }

      for(auto I = BB->begin(), E = BB->end(); I != E; ++I){
         if(StoreInst* SI = dyn_cast<StoreInst>(I)){
            Instruction* LHS = dyn_cast<Instruction>(SI->getOperand(0));
            Instruction* RHS = dyn_cast<Instruction>(SI->getOperand(1));
            if(LHS && RHS && MarkPreserve::is_marked(LHS) && MarkPreserve::is_marked(RHS))
               MarkPreserve::mark(SI, "store");
         }

         if(CallInst* CI = dyn_cast<CallInst>(I)){
            Function* Func = dyn_cast<Function>(castoff(CI->getCalledValue()));
            if(!Func) continue;
            if(!Func->empty()){  /* a func's body is empty, means it is not a
                                    native function */
               MarkPreserve::mark_all(CI, RP.getResolver<NoResolve>(), "callgraph");
            }else{  /* to make sure whether this is an *important* function, we
                       need lookup a dictionary, in there, we use a libcall to
                       do it */
               auto FuncInfo = LibCall.LCI->getFunctionInfo(Func);
               if(FuncInfo && FuncInfo->UniversalBehavior &
                     AliasAnalysis::ModRefResult::Mod){ /* the function writes
                                                           memory */
                  MarkPreserve::mark_all(CI, RP.getResolver<NoResolve>(), "callgraph");
               }
            }
         }
      }
   }

   for(auto BB = F.begin(), E = F.end(); BB !=E; ++BB){
      for(auto I = BB->begin(), E = BB->end(); I != E; ++I){
         if(MarkPreserve::is_marked(I))
            MarkPreserve::mark_all(I, RP.getResolver<NoResolve>(), "closure");
      }
   }

   if(ShrinkLevel == 0) return false;

   if((IgnoreFunc.count(F.getName())) ^ // when ShrinkLevel == 2, Reverse, only process IgnoredFunc
         (ShrinkLevel==2)) return false; /* some initial and import
      code are in main function which is in IgnoreFunc file. so we don't shrink
      it. this is triggy. and the best way is to automatic indentify which a
      function or a part of function is important*/

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

char ReduceCode::ID = 0;

static AttributeFlags gfortran_write_stdout(llvm::CallInst* CI)
{
   Use& st_parameter = CI->getArgOperandUse(0);
   //find_element_store(st_parameter, 0, 0, 1);
   return IsPrint;
}

ReduceCode::ReduceCode():ModulePass(ID)
{
   Attributes["_gfortran_transfer_character_write"] = gfortran_write_stdout;
   Attributes["_gfortran_transfer_integer_write"] = gfortran_write_stdout;
   Attributes["_gfortran_transfer_real_write"] = gfortran_write_stdout;
   Attributes["_gfortran_st_write"] = gfortran_write_stdout;
   Attributes["_gfortran_st_write_done"] = gfortran_write_stdout;
}

void ReduceCode::getAnalysisUsage(AnalysisUsage& AU) const
{
   AU.setPreservesAll();
}

bool ReduceCode::runOnModule(Module &M)
{
   return true;
}
