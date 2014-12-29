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
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/IPO.h>

#include <ValueProfiling.h>

#include "ddg.h"
#include "LoopTripCount.h"
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
static AttributeFlags noused_exclude(llvm::Use& U, llvm::SmallPtrSetImpl<User*>& exclude);
static AttributeFlags noused(llvm::Use& U, ResolveEngine::CallBack C = ResolveEngine::always_false);
#ifndef NDEBUG
static void noused_value(llvm::Value* I, ResolveEngine::CallBack C = ResolveEngine::always_false);
static bool Dbg_EnablePrintGraph = false;
#endif


AttributeFlags ReduceCode::getAttribute(CallInst * CI) const
{
   StringRef Name = castoff(CI->getCalledValue())->getName();
   auto Found = Attributes.find(Name);
   if(Found == Attributes.end()) return AttributeFlags::None;
   return Found->second(CI);
}

void ReduceCode::undefParameter(CallInst* CI)
{
   Function* F = dyn_cast<Function>(castoff(CI->getCalledValue()));
   if(F==NULL || F->isDeclaration()) return;
   errs()<<*CI<<"\n";

   ResolveEngine RE;
   RE.addRule(RE.ibase_rule);
   GEPFilter gep_fi(nullptr);
   RE.addFilter(std::ref(gep_fi));
   CGFilter cg_fi(root, DomT, CI);
   RE.addFilter(std::ref(cg_fi));

   for(auto& Op : CI->arg_operands()){
      GlobalVariable* GV;
      GetElementPtrInst* GEP;
      if(isRefGlobal(Op.get(), &GV, &GEP)){
         gep_fi = GEPFilter(GEP);
         if(GEP) errs()<<*GEP<<"\n";
         auto ddg = RE.resolve(GV);
         WriteGraph(&ddg, "test");
      }
   }
}

bool ReduceCode::runOnFunction(Function& F)
{
   LoopTripCount& TC = getAnalysis<LoopTripCount>(F);
   DomT = &getAnalysis<DominatorTreeWrapperPass>(F).getDomTree();
   dse.prepare(&F, this);
   BasicBlock* entry = &F.getEntryBlock();
   std::vector<BasicBlock*> blocks(po_begin(entry), po_end(entry));
   bool MadeChange, Ret = false;
   // use deep first visit order , then reverse it. can get what we need order.
   for(auto BB = blocks.rbegin(), BBE = blocks.rend(); BB != BBE;){
      dse.runOnBasicBlock(**BB);
      MadeChange = false;
      for(auto I = --(*BB)->end(), IE = --(*BB)->begin(); I!=IE;){
         Instruction* Inst = &*(I--);
         AttributeFlags flag = AttributeFlags::None;
         if(CallInst* CI = dyn_cast<CallInst>(Inst)){
            flag = getAttribute(CI);
            if(flag == AttributeFlags::None)
               undefParameter(CI);
         }else if(StoreInst* SI = dyn_cast<StoreInst>(Inst)){

            if(Argument* A = dyn_cast<Argument>(SI->getPointerOperand())){
               llvm::SmallSet<User*, 3> S;
               insert_to(F.user_begin(), F.user_end(), S);
               bool all_deletable = std::all_of(S.begin(), S.end(), [A, &S](User* C){
                     llvm::Use* Para = findCallInstParameter(A, dyn_cast<CallInst>(C));
                     if(Para == NULL) return 0;
                     return (noused_exclude(*Para, S) & IsDeletable);
                     });
               //StoreInst never cascade, so IsDeletable is enough
               flag = all_deletable ? IsDeletable : AttributeFlags::None;
            }else if(isa<AllocaInst>(SI->getPointerOperand())){
               Loop* L = TC.getLoopFor(SI->getParent());
               // a store inst in loop, and it isn't used after loop
               // and it isn't induction, then it can be removed
               if(L && TC.getInduction(L) != NULL){
                  Value* Ind = cast<Instruction>(TC.getInduction(L));
                  if(LoadInst* LI = dyn_cast<LoadInst>(Ind))
                     Ind = LI->getOperand(0);
                  if(Ind != SI->getPointerOperand()){
                     //XXX not stable, because it doesn't use domtree info
                     flag = noused(SI->getOperandUse(1));
                  }
               }else{
                  // a store inst not in loop, and it isn't used after
                  // then it can be removed
                  flag = noused(SI->getOperandUse(1));
               }
            }
         }
         if(flag & IsDeletable){
            (flag & Cascade)? dse.DeleteCascadeInstruction(Inst): 
               dse.DeleteDeadInstruction(Inst);
            Ret = MadeChange = true;
            break;
         }
      }
      // if made change, we always recaculate this BB.
      // if not, we goto next BB.
      if(!MadeChange) ++BB;
   }
   return Ret;
}

void ReduceCode::walkThroughCg(llvm::CallGraphNode * CGN)
{
   Function* F = CGN->getFunction();
   if(!F || F->isDeclaration()) return; // this is a external function

   runOnFunction(*F);
   washFunction(F);

   CallGraphNode::CalledFunctionsVector vec(CGN->begin(), CGN->end());
   for(auto I = vec.rbegin(), E = vec.rend(); I!=E; ++I)
      walkThroughCg(I->second);

   runOnFunction(*F);
   washFunction(F);

   // eliminate function's argument
   dae.runOnFunction(*F);
   deleteDeadCaller(F);
}

bool ReduceCode::runOnModule(Module &M)
{
   dse.prepare(this);
   dae.prepare(&M);
   ic.prepare(this);
   simpCFG.prepare(this);

   CallGraph CG(M);
   Function* Main = M.getFunction("main");
   root = Main?CG[Main]:CG.getExternalCallingNode();

   walkThroughCg(root);
   return true;
}
void ReduceCode::getAnalysisUsage(AnalysisUsage& AU) const
{
   AU.addRequired<LoopTripCount>();
   AU.addRequired<DominatorTreeWrapperPass>();
   dse.getAnalysisUsage(AU);
   ic.getAnalysisUsage(AU);
   simpCFG.getAnalysisUsage(AU);
}

void ReduceCode::washFunction(llvm::Function *F)
{
   ic.runOnFunction(*F);
   simpCFG.runOnFunction(*F);
}

void ReduceCode::deleteDeadCaller(llvm::Function *F)
{
   //FunctionPass& IC = getAnalysisID<FunctionPass>(ic.id(), *F);
   for(auto I = F->use_begin(), E = F->use_end(); I!=E; ++I){
      CallInst* CI = dyn_cast<CallInst>(I->getUser());
      if(CI == NULL) continue;
      BasicBlock* BB = CI->getParent();
      Function* Parent = BB->getParent();
      washFunction(Parent);
   }
}


//======================ATTRIBUTE BEGIN====================================//
static AttributeFlags gfortran_write_stdout(llvm::CallInst* CI)
{
   Use& st_parameter = CI->getArgOperandUse(0);
   ResolveEngine RE;
   RE.addRule(RE.base_rule);
   RE.addRule(RE.gep_rule);
   RE.addRule(RE.useonly_rule);
   RE.addFilter(GEPFilter{0,0,1});
   Value* Store = RE.find_store(st_parameter);
   if(Store != NULL && isa<StoreInst>(Store)){
      auto u = extract(dyn_cast<ConstantInt>(cast<StoreInst>(Store)->getValueOperand()));
      if(u == 6 || u == 0) return IsPrint; // 6 means write to stdout, 0 means write to stderr
   }
   return AttributeFlags::None;
}

#ifndef NDEBUG
static void noused_value(llvm::Value* U, ResolveEngine::CallBack C)
{
   ResolveEngine RE;
   RE.addRule(RE.ibase_rule);
   InitRule ir(RE.iuse_rule);
   RE.addRule(std::ref(ir));
   if(Dbg_EnablePrintGraph){
      auto ddg = RE.resolve(U, C);
      Instruction* I = dyn_cast<Instruction>(U);
      BasicBlock* Block = I?I->getParent():NULL;
      StringRef FName = Block?Block->getParent()->getName():"test";
      WriteGraph(&ddg, FName);
   }
}
#endif

static AttributeFlags noused(llvm::Use& U, ResolveEngine::CallBack C)
{
   ResolveEngine RE;
   RE.addRule(RE.ibase_rule);
   InitRule ir(RE.iuse_rule);
   RE.addRule(std::ref(ir));
   Value* Visit = RE.find_visit(U, C);
   ir.clear();
#ifndef NDEBUG
   if(Dbg_EnablePrintGraph){
      auto ddg = RE.resolve(U, C);
      Instruction* I = dyn_cast<Instruction>(U.getUser());
      BasicBlock* Block = I?I->getParent():NULL;
      StringRef FName = Block?Block->getParent()->getName():"test";
      WriteGraph(&ddg, FName);
   }
#endif
   if(Visit == NULL) return IsDeletable;
   else return AttributeFlags::None;
}

static AttributeFlags mpi_nouse_at(llvm::CallInst* CI, unsigned Which)
{
   Use& buf = CI->getArgOperandUse(Which);
   return noused(buf);
}

static AttributeFlags noused_exclude(llvm::Use& U, llvm::SmallPtrSetImpl<User*>& exclude)
{
   User* Self = U.getUser();
   AttributeFlags ret = noused(U, [&exclude, Self](Use* U){
         if(U->getUser() != Self && exclude.count(U->getUser())) return true;
         return false;
         });
   return ret;
}

static constexpr AttributeFlags direct_return(CallInst* CI, AttributeFlags flags)
{
   return flags;
}


ReduceCode::ReduceCode():ModulePass(ID), 
   dse(createDeadStoreEliminationPass()),
   dae(createDeadArgEliminationPass()),
   ic(createInstructionCombiningPass()),
   simpCFG(createCFGSimplificationPass())
{
   using std::placeholders::_1;
   Attributes["_gfortran_transfer_character_write"] = gfortran_write_stdout;
   Attributes["_gfortran_transfer_integer_write"] = gfortran_write_stdout;
   Attributes["_gfortran_transfer_real_write"] = gfortran_write_stdout;
   Attributes["_gfortran_st_write"] = gfortran_write_stdout;
   Attributes["_gfortran_st_write_done"] = gfortran_write_stdout;
//int MPI_Reduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
//               MPI_Op op, int root, MPI_Comm comm)
//Deletable if recvbuf is no used
   Attributes["mpi_reduce_"] = std::bind(mpi_nouse_at, _1, 1);
//int MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
//             MPI_Comm comm)
//Deletable if buf is no used
   Attributes["mpi_send_"] = std::bind(mpi_nouse_at, _1, 0);
//int MPI_Irecv(void *buf, int count, MPI_Datatype datatype, int source,
//              int tag, MPI_Comm comm, MPI_Request *request)
//Deletable if buf is no used
   Attributes["mpi_irecv_"] = std::bind(mpi_nouse_at, _1, 0);
   //always delete mpi_wtime_
   Attributes["mpi_wtime_"] = std::bind(direct_return, _1, 
         AttributeFlags::IsDeletable | AttributeFlags::Cascade);
   Attributes["mpi_wait_"] = std::bind(direct_return, _1, AttributeFlags::IsDeletable);
   Attributes["main"] = std::bind(direct_return, _1, AttributeFlags::None);
}

//==================================ATTRIBUTE END===============================//
