#include "preheader.h"
#include "Resolver.h"
#include "KnownLibCallInfo.h"

#include <fstream>
#include <stdlib.h>
#include <unordered_set>

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/GraphWriter.h>
#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/DepthFirstIterator.h>
#include <llvm/Analysis/LibCallAliasAnalysis.h>

#include <ValueProfiling.h>

#include "ddg.h"
#include "SlashShrink.h"
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
static RegisterPass<ReduceCode> Y("Reduce", "Slash and Shrink Code to make a minicore program");


AttributeFlags ReduceCode::getAttribute(CallInst * CI) const
{
   StringRef Name = castoff(CI->getCalledValue())->getName();
   auto Found = Attributes.find(Name);
   if(Found == Attributes.end()) return AttributeFlags::None;
   return Found->second(CI);
}

bool ReduceCode::runOnModule(Module &M)
{
   CallGraph CG(M);
   Function* Main = M.getFunction("main");
   CallGraphNode* root = Main?CG[Main]:CG.getExternalCallingNode();
   std::vector<CallGraphNode*> nodes(df_begin(root), df_end(root));
   // use deep first visit order , then reverse it. can get what we need order.
   for(auto FI = nodes.rbegin(), FE = nodes.rend(); FI != FE; ++FI){
      Function * F = (*FI)->getFunction();
      if(F==NULL || F->isDeclaration()) continue; //F==NULL --> is a external node
      BasicBlock* entry = &F->getEntryBlock();
      std::vector<BasicBlock*> blocks(po_begin(entry), po_end(entry));
      // use deep first visit order , then reverse it. can get what we need order.
      for(auto BB = blocks.rbegin(), BBE = blocks.rend(); BB != BBE; ++BB){
         for(auto I = --(*BB)->end(), IE = --(*BB)->begin(); I!=IE;){
            Instruction* Inst = &*(I--);
            if(CallInst* CI = dyn_cast<CallInst>(Inst)){
               if(getAttribute(CI) == IsDeletable){
                  deleteInst(CI);
               }
            }
         }
      }
   }

   return true;
}
void ReduceCode::getAnalysisUsage(AnalysisUsage& AU) const
{
   AU.setPreservesAll();
}

void ReduceCode::deleteInst(Instruction * I)
{
   I->replaceAllUsesWith(UndefValue::get(I->getType()));
   for(unsigned i=0;i<I->getNumOperands();++i)
      I->setOperand(i, NULL); /* destroy instruction need clean holds
                                 reference */
   I->removeFromParent(); /* use erase from would cause crash let
                                 it freed by Context */
}


//======================ATTRIBUTE BEGIN====================================//
static AttributeFlags gfortran_write_stdout(llvm::CallInst* CI)
{
   Use& st_parameter = CI->getArgOperandUse(0);
   ResolveEngine RE;
   RE.addRule(RE.base_rule);
   RE.addRule(RE.gep_rule);
   RE.addRule(RE.useonly_rule);
   Value* Store = RE.find_store(st_parameter, [](Use* U){
         if(auto GEP = dyn_cast<GetElementPtrInst>(U->getUser())){
            if(GEP->getOperand(0) == U->get() && GEP->getNumIndices() == 3){
               if(equal(dyn_cast<ConstantInt>(GEP->getOperand(1)), 0) &&
                     equal(dyn_cast<ConstantInt>(GEP->getOperand(2)), 0) &&
                     equal(dyn_cast<ConstantInt>(GEP->getOperand(3)), 1) )
                  return false;
            }
            return true;
         }
         return false;
         });
   if(Store != NULL){
      auto u = extract(dyn_cast<ConstantInt>(Store));
      if(u == 6 || u == 0) return IsPrint; // 6 means write to stdout, 0 means write to stderr
   }
   return AttributeFlags::None;
}

static AttributeFlags mpi_reduce_nodep(llvm::CallInst* CI)
{
   Use& recvbuf = CI->getArgOperandUse(1);
   ResolveEngine RE;
   RE.addRule(RE.ibase_rule);
   RE.addRule(InitRule(RE.iuse_rule));
   DDGraph G = RE.resolve(recvbuf);
   WriteGraph(&G, "ddg", false);
   return AttributeFlags::None;
}

static AttributeFlags direct_return(CallInst* CI, AttributeFlags flags)
{
   return flags;
}


ReduceCode::ReduceCode():ModulePass(ID)
{
   using std::placeholders::_1;
   Attributes["_gfortran_transfer_character_write"] = gfortran_write_stdout;
   Attributes["_gfortran_transfer_integer_write"] = gfortran_write_stdout;
   Attributes["_gfortran_transfer_real_write"] = gfortran_write_stdout;
   Attributes["_gfortran_st_write"] = gfortran_write_stdout;
   Attributes["_gfortran_st_write_done"] = gfortran_write_stdout;
   Attributes["mpi_reduce_"] = mpi_reduce_nodep;
   Attributes["main"] = std::bind(direct_return, _1, AttributeFlags::None);
}

//==================================ATTRIBUTE END===============================//
