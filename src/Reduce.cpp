#include "preheader.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Support/GraphWriter.h>
#include <llvm/ADT/PostOrderIterator.h>

#include <ValueProfiling.h>

#include "LoopTripCount.h"
#include "Resolver.h"
#include "Reduce.h"
#include "ddg.h"
#include "debug.h"

#define REASON(flag, what, tag) DEBUG(if(flag){\
      errs()<<(what)<<" removed in line: "<<__LINE__<<':'<<tag<<"\n";\
      errs()<<"which with "<<*(what).getPointerOperand()<<"\n";\
      })

using namespace std;
using namespace lle;
using namespace llvm;

char ReduceCode::ID = 0;
static RegisterPass<ReduceCode> Y("Reduce", "Slash and Shrink Code to make a minicore program");
static AttributeFlags noused_exclude(llvm::Use& U, llvm::SmallPtrSetImpl<User*>& exclude);
static AttributeFlags noused(llvm::Use& U, ResolveEngine::CallBack C = ResolveEngine::always_false);
cl::opt<bool> Force("Force", cl::desc("Enable Force Reduce Mode"));
#ifndef NDEBUG
static bool Dbg_EnablePrintGraph = false;
static void Dbg_PrintGraph_(DataDepGraph&& ddg, User* Ur)
{
   Instruction* I = Ur?dyn_cast<Instruction>(Ur):NULL;
   BasicBlock* Block = I?I->getParent():NULL;
   StringRef FName = Block?Block->getParent()->getName():"test";
   WriteGraph(&ddg, FName);
}
#define Dbg_PrintGraph(ddg, Ur) if(Dbg_EnablePrintGraph) Dbg_PrintGraph_(ddg, Ur)
#else
#define Dbg_PrintGraph(ddg, Ur)
#endif


AttributeFlags ReduceCode::getAttribute(CallInst * CI)
{
   StringRef Name = castoff(CI->getCalledValue())->getName();
   auto Found = Attributes.find(Name);
   if(Found == Attributes.end()) return AttributeFlags::None;
   return Found->second(CI);
}

AttributeFlags ReduceCode::noused_param(Argument* Arg)
{
   Function* F = Arg->getParent();
   llvm::SmallSet<User*, 3> S;
   insert_to(F->user_begin(), F->user_end(), S);
   ReduceCode* Self = this;
   bool all_deletable = std::all_of(F->user_begin(), F->user_end(), [Self, Arg, &S](User* C){
         CallInst* CI = dyn_cast<CallInst>(C);
         llvm::Use* Para = findCallInstParameter(Arg, CI);
         if(Para == NULL) return 0;
         auto f = (noused_exclude(*Para, S) & IsDeletable);
         Argument* NestArg = dyn_cast<Argument>(Para->get());
         if(f && NestArg) f &= Self->noused_param(NestArg);
         GlobalVariable* GV = NULL;
         GetElementPtrInst* GEP = NULL;
         if(f && isRefGlobal(Para->get(), &GV, &GEP))
            f &= Self->noused_global(GV, CI);
         return f;
         });
   return all_deletable ? IsDeletable : AttributeFlags::None;
}

static AttributeFlags noused(llvm::Value* V)
{
   ResolveEngine RE;
   RE.addRule(RE.ibase_rule);
   Value* Visit = RE.find_visit(V);
   if(Visit == NULL) return IsDeletable;
   else return AttributeFlags::None;
}
static AttributeFlags noused_ret_rep(ReturnInst* RI)
{
   Value* Ret = RI->getReturnValue();
   if(Ret == NULL || isa<UndefValue>(Ret)) return AttributeFlags::None;

   Function* F = RI->getParent()->getParent();
   if(F->getName() == "main") return AttributeFlags::None;

   bool all_deletable = std::all_of(F->user_begin(), F->user_end(), [](User* C){
         return C->use_empty();
         });
   if(all_deletable)
      ReturnInst::Create(F->getContext(), UndefValue::get(Ret->getType()), RI->getParent());
   return all_deletable ? IsDeletable : AttributeFlags::None;
}
AttributeFlags ReduceCode::noused_global(GlobalVariable* GV, Instruction* GEP)
{
   ResolveEngine RE;
   RE.addRule(RE.ibase_rule);
   GetElementPtrInst* GEPI = dyn_cast<GetElementPtrInst>(GEP);
   if(GEPI) RE.addFilter(GEPFilter(GEPI));
   RE.addFilter(CGFilter(root, GEP));
   Value* Visit = RE.find_visit(GV);
   Dbg_PrintGraph(RE.resolve(GV), nullptr);
   if(Visit == NULL) return AttributeFlags::IsDeletable;
   else return AttributeFlags::None;
}

AttributeFlags ReduceCode::getAttribute(StoreInst *SI)
{
   AttributeFlags flag = AttributeFlags::None;
   Argument* Arg = dyn_cast<Argument>(SI->getPointerOperand());
   AllocaInst* Alloca = dyn_cast<AllocaInst>(SI->getPointerOperand());
   auto GEP = dyn_cast<GetElementPtrInst>(SI->getPointerOperand());

   if(Protected.count(SI)) return AttributeFlags::None;

   if(GEP){
      Arg = dyn_cast<Argument>(GEP->getPointerOperand());
      Alloca = dyn_cast<AllocaInst>(GEP->getPointerOperand());
      // 过于激进的删除
      if(GlobalVariable* GV = dyn_cast<GlobalVariable>(GEP->getPointerOperand())){
         flag = noused_global(GV, GEP);
         REASON(flag, *SI, 0);
         return flag;
      }
   }

   if(Arg){
      flag = noused(SI->getOperandUse(1));
      flag = AttributeFlags(flag & noused_param(Arg));
      REASON(flag, *SI, (GEP==nullptr));
   }else if(Alloca){
      Loop* L = LTC->getLoopFor(SI->getParent());
      if(L){
         Value* Ind = LTC->getInduction(L);
         if(Ind != NULL){
            // a store inst in loop, and it isn't used after loop
            // and it isn't induction, then it can be removed
            if(LoadInst* LI = dyn_cast<LoadInst>(Ind))
               Ind = LI->getOperand(0);
            if(Ind != SI->getPointerOperand()){
               //XXX not stable, because it doesn't use domtree info
               flag = noused(SI->getOperandUse(1));
               REASON(flag, *SI, (GEP==nullptr));
            }
         }// if it is in loop, and we can't get induction, we ignore it
      }else{
         // a store inst not in loop, and it isn't used after
         // then it can be removed
         flag = noused(SI->getOperandUse(1));
         REASON(flag, *SI, (GEP==nullptr));
      }
   }
   return flag;
}

bool ReduceCode::runOnFunction(Function& F)
{
   LTC = &getAnalysis<LoopTripCount>(F);
   DomT = &getAnalysis<DominatorTreeWrapperPass>(F).getDomTree();
   dse.prepare(&F, this);
   BasicBlock* entry = &F.getEntryBlock();
   std::vector<BasicBlock*> blocks(po_begin(entry), po_end(entry));
   bool MadeChange, Ret = false;
   DEBUG(errs()<<F.getName()<<"\n");
   // use deep first visit order , then reverse it. can get what we need order.
   for(auto BB = blocks.begin(), BBE = blocks.end(); BB != BBE;){
      dse.runOnBasicBlock(**BB);
      MadeChange = false;
      for(auto I = --(*BB)->end(), IE = --(*BB)->begin(); I!=IE;){
         Instruction* Inst = &*(I--);
         AttributeFlags flag = AttributeFlags::None;
         if(CallInst* CI = dyn_cast<CallInst>(Inst)){
            flag = getAttribute(CI);
         }else if(ReturnInst* RI = dyn_cast<ReturnInst>(Inst)){
            flag = noused_ret_rep(RI);
         }else if(StoreInst* SI = dyn_cast<StoreInst>(Inst)){
            flag = getAttribute(SI);
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

   deleteDeadCaller(F);
}

static void RemoveDeadFunction(Module& M, bool focusDeclation)
{
   for(auto F = M.begin(), E = M.end(); F!=E;){
      bool ignore = false;
      ignore |= F->getName()=="main";
      ignore |= focusDeclation ^ F->isDeclaration();
      if(!ignore){
         F->removeDeadConstantUsers();
         if(F->use_empty()){
            F = M.getFunctionList().erase(F);
            continue;
         }
      }
      ++F;
   }
}

bool ReduceCode::runOnModule(Module &M)
{
   dse.prepare(this);
   ic.prepare(this);
   simpCFG.prepare(this);

   CallGraph CG(M);
   Function* Main = M.getFunction("main");
   root = Main?CG[Main]:CG.getExternalCallingNode();

   walkThroughCg(root);

   //first remove unused function
   RemoveDeadFunction(M, 0);
   //then remove unused declation
   RemoveDeadFunction(M, 1);
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
   auto ddg = RE.resolve(st_parameter);
   Value* Store = RE.find_store(st_parameter);
   if(Store != NULL && isa<StoreInst>(Store)){
      auto u = extract(dyn_cast<ConstantInt>(cast<StoreInst>(Store)->getValueOperand()));
      if(u == 6 || u == 0) return IsPrint; // 6 means write to stdout, 0 means write to stderr
   }
   return AttributeFlags::None;
}

static AttributeFlags noused(llvm::Use& U, ResolveEngine::CallBack C)
{
   ResolveEngine RE;
   RE.addRule(RE.ibase_rule);
   InitRule ir(RE.iuse_rule);
   RE.addRule(std::ref(ir));
   Value* Pointed = U.get();
   Use* ToSearch = &U;
   if(RE.find_visit(*ToSearch, C)) return AttributeFlags::None;
   if(auto GEP = dyn_cast<GetElementPtrInst>(Pointed)){
      // if we didn't find direct visit on Pointed, we tring find visit on
      // GEP->getPointerOperand()
      RE.addFilter(GEPFilter(GEP));
      ToSearch = &GEP->getOperandUse(0);
      ir.clear();
      if(RE.find_visit(*ToSearch, C)) return AttributeFlags::None;
      else {
         ir.clear();
         Dbg_PrintGraph(RE.resolve(*ToSearch, C), U.getUser());
      }
   }
   ir.clear();
   Dbg_PrintGraph(RE.resolve(U, C), U.getUser());
   return AttributeFlags::IsDeletable;
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

static AttributeFlags mpi_comm_replace(CallInst* CI, SmallPtrSetImpl<StoreInst*>* Protected, const char* Env)
{
   Module* M = CI->getParent()->getParent()->getParent();
   LLVMContext& C = M->getContext();
   Type* CharPTy = Type::getInt8PtrTy(C);
   Constant* getenvF = M->getOrInsertFunction("getenv", CharPTy, CharPTy, NULL);
   Constant* atoiF = M->getOrInsertFunction("atoi", Type::getInt32Ty(C), CharPTy, NULL);
   Value* RankVariable = CI->getArgOperand(1);
   Value* Environment = CallInst::Create(getenvF, {insertConstantString(M, Env)}, "", CI);
   Value* Rank = CallInst::Create(atoiF, {Environment}, "", CI);
   Protected->insert(new StoreInst(Rank, RankVariable, CI)); // 因为它是最后加入的, 所以排在use列表的最后.
   return AttributeFlags::IsDeletable;
}

static AttributeFlags mpi_allreduce_force(CallInst* CI)
{
   //FIXME should replaced with memcpy
   Value* Send = CI->getArgOperand(0);
   Value* Recv = CI->getArgOperand(1);

   // if there are free of Send and free of Recv, we replace Recv Use with
   // send, then we free Send twice. this is bad, 
   // we can remove Send's Use for now
   ResolveEngine RE;
   RE.addRule(RE.ibase_rule);
   InitRule ir(RE.iuse_rule);
   RE.addRule(std::ref(ir));
   RE.resolve(CI->getArgOperandUse(0), [](Use* U){
         CallInst* Call = dyn_cast<CallInst>(U->getUser());
         Function* F = NULL;
         if(Call && (F = Call->getCalledFunction())){
            if(F->getName()=="free"){
               Call->setArgOperand(0, UndefValue::get(Call->getArgOperand(0)->getType()));
               Call->removeFromParent();
            }
         }
         return false;
      });
   ir.clear();
   auto ddg = RE.resolve(CI->getArgOperandUse(1), [](Use* U){
         errs()<<*U->getUser()<<"\n";
         CallInst* Call = dyn_cast<CallInst>(U->getUser());
         Function* F = NULL;
         if(Call && (F = Call->getCalledFunction())){
            if(F->getName()=="llvm.lifetime.end"){
               Call->setArgOperand(1, UndefValue::get(Call->getArgOperand(1)->getType()));
               Call->removeFromParent();
            }
         }
         return false;
      });
   WriteGraph(&ddg, "test");

   Recv->replaceAllUsesWith(Send);
   return AttributeFlags::IsDeletable;
}

ReduceCode::ReduceCode():ModulePass(ID), 
   dse(createDeadStoreEliminationPass()),
   ic(createInstructionCombiningPass()),
   simpCFG(createCFGSimplificationPass())
{
   using std::placeholders::_1;
   auto mpi_nouse_recvbuf = std::bind(mpi_nouse_at, _1, 1);
   auto mpi_nouse_buf = std::bind(mpi_nouse_at, _1, 0);
   auto DirectDelete = std::bind(direct_return, _1, AttributeFlags::IsDeletable);
   auto DirectDeleteCascade = std::bind(direct_return, _1, 
         AttributeFlags::IsDeletable | AttributeFlags::Cascade);

   Attributes["_gfortran_transfer_character_write"] = gfortran_write_stdout;
   Attributes["_gfortran_transfer_integer_write"] = gfortran_write_stdout;
   Attributes["_gfortran_transfer_real_write"] = gfortran_write_stdout;
   Attributes["_gfortran_st_write"] = gfortran_write_stdout;
   Attributes["_gfortran_st_write_done"] = gfortran_write_stdout;
   Attributes["_gfortran_system_clock_4"] = DirectDelete;
//int MPI_Reduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
//               MPI_Op op, int root, MPI_Comm comm)
//Deletable if recvbuf is no used
   Attributes["mpi_reduce_"] = mpi_nouse_recvbuf;
//int MPI_Allreduce(const void *sendbuf, void *recvbuf, int count,
//                  MPI_Datatype datatype, MPI_Op op, MPI_Comm comm)
   if(Force) Attributes["mpi_allreduce_"] = mpi_allreduce_force;
   else Attributes["mpi_allreduce_"] = mpi_nouse_recvbuf;
//Deletable if recvbuf is no used
//int MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
//             MPI_Comm comm)
//int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag,
//             MPI_Comm comm, MPI_Status *status)
   Attributes["mpi_send_"] = mpi_nouse_buf;
   Attributes["mpi_recv_"] = mpi_nouse_buf;
//int MPI_Irecv(void *buf, int count, MPI_Datatype datatype, int source,
//              int tag, MPI_Comm comm, MPI_Request *request)
//int MPI_Isend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
//              MPI_Comm comm, MPI_Request *request)
   Attributes["mpi_isend_"] = mpi_nouse_buf;
   Attributes["mpi_irecv_"] = mpi_nouse_buf;
//int MPI_Bcast( void *buffer, int count, MPI_Datatype datatype, int root, 
//               MPI_Comm comm )
   // 由于模拟的时候只有一个进程, 所以不需要扩散变量.
   Attributes["mpi_bcast_"] = /*mpi_nouse_buf;*/DirectDelete;
   Attributes["mpi_comm_rank_"] = std::bind(mpi_comm_replace, _1, &Protected, "MPI_RANK");
   Attributes["mpi_comm_size_"] = std::bind(mpi_comm_replace, _1, &Protected, "MPI_SIZE");
   //always delete mpi_wtime_
   Attributes["mpi_wtime_"] = DirectDeleteCascade;
   Attributes["mpi_error_string_"] = DirectDelete;
   Attributes["mpi_wait_"] = DirectDelete;
   Attributes["mpi_waitall_"] = DirectDelete;
   Attributes["mpi_barrier_"] = DirectDelete;
   Attributes["mpi_init_"] = DirectDelete;
   Attributes["mpi_finalize_"] = DirectDelete;
   Attributes["mpi_abort_"] = DirectDelete;
   Attributes["main"] = std::bind(direct_return, _1, AttributeFlags::None);
}

//==================================ATTRIBUTE END===============================//
