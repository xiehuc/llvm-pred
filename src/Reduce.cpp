#include "preheader.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Support/GraphWriter.h>
#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Analysis/CallGraph.h>

#include <ValueProfiling.h>

#include "LoopTripCount.h"
#include "IgnoreList.h"
#include "Resolver.h"
#include "Reduce.h"
#include "ddg.h"
#include "debug.h"

static unsigned DT[128] = {0};
static int dt_init() {
#include "datatype.h"
   return 0;
}
static int _DT_INIT = dt_init();

// a deeper notice removed object
#define WHAT_RMD(what) {}
#if 0
#define WHAT_RMD(what)                                                         \
   DEBUG({                                                                     \
      errs() << *(what).getUser() << " removed in line ";                      \
      errs() << __LINE__ << ":\n";                                             \
      errs() << " -->" << *(what).get() << "\n";                               \
   })
#endif
#define REAL_RMD(what) DEBUG({ errs() << *(what) << " real removed \n"; })
#ifdef ANNOY_DEBUG
#define WHY_KEPT(what, searched)                                               \
   DEBUG({                                                                     \
      errs() << *(what).getUser() << " couldn't removed because: \n";          \
      errs() << "found visit : " << (*searched) << "\n";                       \
   })
#else
#define WHY_KEPT(what, searched) {}
#endif
#define FLAG(what) (what)?AttributeFlags::None:AttributeFlags::IsDeletable
// a notice according to flag, notice removed object, or kept reason
// @param what: which one removed or kept
// @param searched: if kept, the found user
#define NOTICE(what, searched)                                                 \
   DEBUG({                                                                     \
      if (FLAG(searched))                                                      \
         WHAT_RMD(what)                                                        \
      else                                                                     \
         WHY_KEPT(what, searched)                                              \
   })
// should removed when all noused_* return a user insead of flag
#define WHY_RMED(flag, what) DEBUG(if (flag) WHAT_RMD(what))

using namespace std;
using namespace lle;
using namespace llvm;

static const std::set<StringRef> MpiDelayDelete
    = { "mpi_init_", "mpi_finalize_", "mpi_comm_rank_", "mpi_comm_size_",
        "mpi_barrier_" };

char ReduceCode::ID = 0;
static RegisterPass<ReduceCode> Y("Reduce", "Slash and Shrink Code to make a minicore program");
cl::opt<bool> Force("Force", cl::desc("Enable Force Reduce Mode"));
#ifndef NDEBUG
static bool Dbg_EnablePrintGraph = false;
static void Dbg_PrintGraph_(DataDepGraph&& ddg, User* Ur)
{
   Instruction* I = Ur ? dyn_cast<Instruction>(Ur) : NULL;
   BasicBlock* Block = I ? I->getParent() : NULL;
   StringRef FName = Block ? Block->getParent()->getName() : "test";
   WriteGraph(&ddg, FName);
}
#define Dbg_PrintGraph(ddg, Ur) if(Dbg_EnablePrintGraph) Dbg_PrintGraph_(ddg, Ur)
#else
#define Dbg_PrintGraph(ddg, Ur)
#endif

struct excluded{
   llvm::SmallPtrSetImpl<User*>& ex;
   excluded(llvm::SmallPtrSetImpl<User*>& e):ex(e) {}
   bool operator()(Use* U){
      if(ex.count(U->getUser())) return true;
      return false;
   }
};

// a simple basic noused check 
static AttributeFlags noused_flat(llvm::Use& U, ResolveEngine::CallBack C = ResolveEngine::always_false)
{
   static bool inited = false;
   static ResolveEngine RE;
   static InitRule ir(RE.iuse_rule);
   static ResolveCache RC1, RC2, RC3;
   if(!inited){
     RE.addRule(RE.ibase_rule);
     RE.addRule(std::ref(ir));
     inited = true;
   }

   Use* ToSearch = &U;
   Value* Searched;
   RE.clearFilters();
   RE.addFilter(C);
   RE.addFilter(iUseFilter(&U));
   RE.addFilter(RE.exclude(&U));
   RE.useCache(RC1);
   ir.clear();
   RE.resolve(ToSearch, RE.findVisit(Searched));
   if(Searched){
      WHY_KEPT(U, Searched);
      return AttributeFlags::None;
   }
   if(auto CAST = isCast_(U)){
      ToSearch = &CAST->getOperandUse(0);
      ir.clear();
      RE.useCache(RC3);
      RE.resolve(ToSearch, RE.findVisit(Searched));
      if (Searched) {
         WHY_KEPT(U, Searched);
         return AttributeFlags::None;
      } else {
         ir.clear();
         Dbg_PrintGraph(RE.resolve(ToSearch), U.getUser());
      }
   }
   if(auto GEP = isGEP(U)){
      // if we didn't find direct visit on Pointed, we tring find visit on
      // GEP->getPointerOperand()
      unsigned notused;
      if(!RC2.ask(&U, Searched, notused)){
         RE.addFilter(GEPFilter(GEP));
         ToSearch = &GEP->getOperandUse(0);
         ir.clear();
         RE.resolve(ToSearch, RE.findVisit(Searched));
         RC2.storeKey(&U);
         RC2.storeValue(Searched, 0);
      }
      if (Searched) {
         WHY_KEPT(U, Searched);
         return AttributeFlags::None;
      } else {
         ir.clear();
         Dbg_PrintGraph(RE.resolve(ToSearch), U.getUser());
      }
   }
   ir.clear();
   Dbg_PrintGraph(RE.resolve(&U), U.getUser());
   WHAT_RMD(U);
   return AttributeFlags::IsDeletable;
}

AttributeFlags ReduceCode::getAttribute(CallInst * CI)
{
   StringRef Name = castoff(CI->getCalledValue())->getName();
   string NameRef;
   if(auto I = dyn_cast<IntrinsicInst>(CI))
      Name = NameRef = getName(I->getIntrinsicID()); // nameref hold a real string
   auto Found = Attributes.find(Name);
   if(Found == Attributes.end()) return AttributeFlags::None;
   auto Ret = Found->second(CI);
   if(Ret & AttributeFlags::IsDeletable && !MpiDelayDelete.count(Name))
     mpi_stats.unref(CI);
   return Ret;
}

AttributeFlags ReduceCode::noused_param(Argument* Arg)
{
   Function* F = Arg->getParent();
   llvm::SmallSet<User*, 3> S;// should we delete it?
   insert_to(F->user_begin(), F->user_end(), S);
   ReduceCode* Self = this;
   bool all_deletable = std::all_of(F->user_begin(), F->user_end(), [Self, Arg, &S](User* C){
         CallInst* CI = dyn_cast<CallInst>(C);
         llvm::Use* Para = findCallInstParameter(Arg, CI);
         if(Para == NULL) return 0;
         auto f = (noused_flat(*Para, excluded(S)) & IsDeletable);
         Argument* NestArg = dyn_cast<Argument>(Para->get());
         if(f && NestArg) f &= Self->noused_param(NestArg);
         GlobalVariable* GV = NULL;
         Use* GEP = NULL;
         if(f && isRefGlobal(Para->get(), &GV, &GEP))
            f &= FLAG(Self->noused_global(GV, CI, GEP, excluded(S)));
         return f;
         });
   return all_deletable ? IsDeletable : AttributeFlags::None;
}

static AttributeFlags noused(llvm::Value* V)
{
   ResolveEngine RE;
   RE.addRule(RE.ibase_rule);
   Value* Ref;
   RE.resolve(V, RE.findRef(Ref));
   if(Ref == NULL) return IsDeletable;
   else return AttributeFlags::None;
}
static AttributeFlags noused_ret_rep(ReturnInst* RI)
{
   Value* Ret = RI->getReturnValue();
   if(Ret == NULL || isa<UndefValue>(Ret)) return AttributeFlags::None;

   Function* F = RI->getParent()->getParent();
   if(F->getName() == "main") return AttributeFlags::None;

   bool all_deletable = std::all_of(F->user_begin(), F->user_end(), [](User* C){
         return noused(C);//find_visit doesn't consider store, we temporary doesn't use it
         });
   if(all_deletable)
      ReturnInst::Create(F->getContext(), UndefValue::get(Ret->getType()), RI->getParent());
   return all_deletable ? IsDeletable : AttributeFlags::None;
}
Value* ReduceCode::noused_global(GlobalVariable* GV, Instruction* pos, Use* GEP, ResolveEngine::CallBack C)
{
   static ResolveEngine RE;
   static bool inited = false;
   static ResolveCache RC;
   if(!inited){
      RE.addRule(RE.ibase_rule);
      inited = true;
   }
   RE.clearFilters();
   CGF->update(pos);
   RE.addFilter(*CGF);
   Value* Visit;
   unsigned op;
   if(GEP){
      RE.addFilter(GEPFilter(dyn_cast<User>(GEP->get())));
      // we need the real address GEP(Inst or ConstantExpr) to lookup, 
      // so it shouldn't use ConstantExpr::getAsInstruction's return
      // because this address is not stable
      if(RC.ask(GEP, Visit, op))
         return Visit;
   }
   RE.addFilter(C);
   RE.resolve(GV, RE.findVisit(Visit));
   if(GEP && Visit){
      RC.storeKey(GEP);
      RC.storeValue(Visit, 0);
   }
   Dbg_PrintGraph(RE.resolve(GV), nullptr);
   return Visit;
}
AttributeFlags ReduceCode::nousedOperator(Use& op, Instruction* pos, ConfigFlags c)
{
   AttributeFlags flag = AttributeFlags::None;
   Value* what;
   Value* target = op.get();
   Argument* Arg = dyn_cast<Argument>(target);
   AllocaInst* Alloca = dyn_cast<AllocaInst>(target);
   auto GEP = isGEP(op);

   if(GEP){
      Arg = dyn_cast<Argument>(GEP->getOperand(0));
      Alloca = dyn_cast<AllocaInst>(GEP->getOperand(0));
      // 过于激进的删除
      if(GlobalVariable* GV = dyn_cast<GlobalVariable>(GEP->getOperand(0))){
         if(GV->getName().endswith("Counters")) return AttributeFlags::None;
         what = noused_global(GV, pos, GEP?&op:NULL);
         NOTICE(op, what);
         return FLAG(what);
      }
   }if(Arg){
      flag = noused_flat(op);
      flag = AttributeFlags(flag & noused_param(Arg));
      WHY_RMED(flag, op);
      return flag;
   }else if(Alloca){
      // a store inst not in loop, and it isn't used after
      // then it can be removed
      Loop* L = NULL;
      if (!(c & DISABLE_STORE_INLOOP)
          && (L = LTC->getLoopFor(pos->getParent()))) {
         Value* Ind = LTC->getInduction(L);
         if(Ind != NULL){
            // a store inst in loop, and it isn't used after loop
            // and it isn't induction, then it can be removed
            if(LoadInst* LI = dyn_cast<LoadInst>(Ind))
               Ind = LI->getOperand(0);
            if(Ind == target){
               //XXX not stable, because it doesn't use domtree info
               return AttributeFlags::None;
            }
         }// if it is in loop, and we can't get induction, we ignore it
      }
      // a store inst not in loop, and it isn't used after
      // then it can be removed
   }
   // in other case
   flag = noused_flat(op);
   WHY_RMED(flag, op);
   return flag;
}
AttributeFlags ReduceCode::getAttribute(StoreInst *SI)
{
   if(Protected.count(SI)) return AttributeFlags::None;
   Use& op = SI->getOperandUse(1);
   // Constant Protection
   if(isa<Constant>(SI->getOperand(0))) return AttributeFlags::None;

   AttributeFlags flag = nousedOperator(op, SI);
   return flag;
}

bool ReduceCode::runOnFunction(Function& F)
{
   if(ignore->count(F.getName())) return false;
   LTC = &getAnalysis<LoopTripCount>(F);
   DomT = &getAnalysis<DominatorTreeWrapperPass>(F).getDomTree();
   dse.prepare(&F, this);
   BasicBlock* entry = &F.getEntryBlock();
   std::vector<BasicBlock*> blocks(po_begin(entry), po_end(entry));
   bool MadeChange, Ret = false;
   DEBUG(errs()<<"==============\n"<<F.getName()<<"\n===============\n");
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
#ifdef DELETE_STORE //delete store inst
         }else if(StoreInst* SI = dyn_cast<StoreInst>(Inst)){
            flag = getAttribute(SI);
#endif
         }
         if(flag & IsDeletable){
            REAL_RMD(Inst);
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

bool ReduceCode::doInitialization(Module& M)
{
  auto DirtyFunc = &this->DirtyFunc;
  for(auto& F : M){
    for(auto& B : F){
      for(auto& I : B){
        CallInst* CI = dyn_cast<CallInst>(&I);
        if(!CI) continue;
        Function* CalledF = dyn_cast<Function>(castoff(CI->getCalledValue()));
        if(!CalledF) continue;
        StringRef FName = CalledF->getName();
        if(!FName.startswith("mpi_")) continue;
        if(MpiDelayDelete.count(FName))
          mpi_stats.onEmpty([DirtyFunc, &F](){
              (*DirtyFunc)[&F] = true;
              });
        else
          mpi_stats.ref(CI);
      }
    }
  }
  return true;
}

bool ReduceCode::doFinalization(Module& M)
{
  FILE* F = fopen("/tmp/lle-all-reduced", "w");
  if(F == NULL){
    perror("could not write reduce result:");
    exit(errno);
  }
  fprintf(F, "%s", mpi_stats.count()?"0":"1");
  fclose(F);
  return true;
}


bool ReduceCode::runOnModule(Module &M)
{
   dae.prepare(&M);
   dse.prepare(this);
   ic.prepare(this);
   simpCFG.prepare(this);

recaculate:
   CallGraph CG(M);
   Function* Main = M.getFunction("main");
   auto root = Main?CG[Main]:CG.getExternalCallingNode();
   CGF = new CGFilter(root,NULL);
   DirtyFunc.clear();
   for(auto& F : M){
      if(!F.isDeclaration() && CGF->count(&F)) DirtyFunc[&F] = true;
   }

   bool Dirty;
   do{
      Dirty = false;
      for(auto I = DirtyFunc.begin(); I!= DirtyFunc.end(); ){
         Function* F = I->first;
         if(I->second == false) {
            ++I;continue;
         }
         if((I->second = runOnFunction(*F))){
            Dirty = true;
            washFunction(F);
            for(auto I = F->use_begin(), E = F->use_end(); I!=E; ++I){
               CallInst* CI = dyn_cast<CallInst>(I->getUser());
               if(CI == NULL) continue;
               Function* Parent = CI->getParent()->getParent();
               DirtyFunc[Parent] = true;
            }
            for(auto I = CG[F]->begin(), E = CG[F]->end(); I!=E; ++I){
               Function* Child = I->second->getFunction();
               if(Child && !Child->isDeclaration()) DirtyFunc[Child] = true;
            }
            string FName = F->getName();
            if(dae.runOnFunction(*F)){
               delete CGF;
               // CG would be auto finalized
               goto recaculate;
            }
         }
      }
   }while(Dirty);

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

void MPIStatistics::ref(llvm::CallInst *CI)
{
  Function* F = dyn_cast<Function>(castoff(CI->getCalledValue()));
  if(!F || !F->getName().startswith("mpi_")) return;
  ++ref_num;
}

void MPIStatistics::unref(llvm::CallInst *CI)
{
  Function* F = dyn_cast<Function>(castoff(CI->getCalledValue()));
  if(!F || !F->getName().startswith("mpi_")) return;
  --ref_num;
  if(ref_num == 0){
    for(auto& F : _on_empty)
      F();
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
   Value* Store;
   RE.resolve(&st_parameter, RE.findStore(Store));
   if(Store != NULL && isa<StoreInst>(Store)){
      auto u = extract(dyn_cast<ConstantInt>(cast<StoreInst>(Store)->getValueOperand()));
      if(u == 6 || u == 0) return IsPrint; // 6 means write to stdout, 0 means write to stderr
   }
   return AttributeFlags::None;
}


static constexpr AttributeFlags direct_return(CallInst* CI, AttributeFlags flags)
{
   return flags;
}

static AttributeFlags mpi_comm_replace(CallInst* CI, SmallPtrSetImpl<StoreInst*>* Protected, MPIStatistics* stat, const std::string Env)
{
   if(stat->count() > 0) return AttributeFlags::None;
   Module* M = CI->getParent()->getParent()->getParent();
   LLVMContext& C = M->getContext();
   Type* CharPTy = Type::getInt8PtrTy(C);
   Type* I32Ty = Type::getInt32Ty(C);
   Instruction* Res;
   Constant* getenvF = M->getOrInsertFunction("getenv", CharPTy, CharPTy, NULL);
   Constant* atoiF = M->getOrInsertFunction("atoi", I32Ty, CharPTy, NULL);
   Constant* putsF = M->getOrInsertFunction("puts", I32Ty, CharPTy, NULL);
   Constant* exitF = M->getOrInsertFunction("exit", Type::getVoidTy(C), I32Ty, NULL);
   Value* RankVariable = CI->getArgOperand(1);
   CallInst* Environment = CallInst::Create(getenvF, {insertConstantString(M, Env)}, "", CI);
   Res = new ICmpInst(CI, ICmpInst::ICMP_EQ, Environment,
         ConstantPointerNull::get(dyn_cast<PointerType>(Environment->getType())));
   CallInst* Rank = CallInst::Create(atoiF, {Environment}, "", CI);
   Res = SplitBlockAndInsertIfThen(Res, Rank, true);
   CallInst::Create(putsF, {insertConstantString(M, 
            "Please set environment "+Env+" variable")},"",Res);
   CallInst::Create(exitF, {ConstantInt::getNullValue(I32Ty)}, "", Res);
   Protected->insert(new StoreInst(Rank, RankVariable, CI)); // 因为它是最后加入的, 所以排在use列表的最后.
   return AttributeFlags::IsDeletable;
}
static AttributeFlags mpi_delay_delete(CallInst* CI, MPIStatistics* stat)
{
  if(stat->count() > 0) return AttributeFlags::None;
  else return AttributeFlags::IsDeletable;
}
template <unsigned send = 0, unsigned recv = 1, unsigned count = 2,
          unsigned type = 3>
static AttributeFlags replace_with_memcpy(CallInst* CI)
{
   Value* Send  = CI->getArgOperand(send);
   Value* Recv  = CI->getArgOperand(recv);
   Value* Count = CI->getArgOperand(count);
   Value* Type  = CI->getArgOperand(type);
   auto GV = dyn_cast<GlobalVariable>(Type);
   auto GVI = dyn_cast_or_null<ConstantInt>(GV?GV->getInitializer():NULL);
   uint64_t TyIdx = GVI?GVI->getZExtValue():7; // DT[7] == 4, default is int
   uint64_t TySize = DT[TyIdx];

   IRBuilder<> Builder(CI);
   Builder.CreateMemCpy(Recv, Send, Builder.CreateLoad(Count), TySize);
   return AttributeFlags::IsDeletable;
}

ReduceCode::ReduceCode()
    : ModulePass(ID)
    , dse(createDeadStoreEliminationPass())
    , dae(createDeadArgEliminationPass())
    , ic(createInstructionCombiningPass())
    , simpCFG(createCFGSimplificationPass())
{
   using std::placeholders::_1;
   ReduceCode* RC = this;
   MPIStatistics* stat = &this->mpi_stats;
   auto nouse_at = [RC, stat](CallInst* CI, unsigned Which) {
      return RC->nousedOperator(CI->getArgOperandUse(Which), CI,
                                DISABLE_STORE_INLOOP);
   };
   auto mpi_nouse_recvbuf = std::bind(nouse_at, _1, 1);
   auto mpi_nouse_buf = std::bind(nouse_at, _1, 0);
   auto DirectDelete = std::bind(direct_return, _1, AttributeFlags::IsDeletable);
   auto DirectDeleteCascade = std::bind(direct_return, _1, 
         AttributeFlags::IsDeletable | AttributeFlags::Cascade);
   auto mpi_direct_delete = std::bind(mpi_delay_delete, _1, stat);

   CGF = NULL;
   ignore = new IgnoreList("FUNC");

   Attributes["_gfortran_transfer_character_write"] = gfortran_write_stdout;
   Attributes["_gfortran_transfer_integer_write"] = gfortran_write_stdout;
   Attributes["_gfortran_transfer_complex_write"] = gfortran_write_stdout;
   Attributes["_gfortran_transfer_real_write"] = gfortran_write_stdout;
   Attributes["_gfortran_st_write"] = gfortran_write_stdout;
   Attributes["_gfortran_st_write_done"] = gfortran_write_stdout;
   Attributes["_gfortran_system_clock_4"] = DirectDelete;
   // memset is write instrincs, we disable gep filter
   // Attributes["llvm.memset"] = std::bind(nouse_at, _1, 0); // XXX: don't delete memset call
   if(Force){
//int MPI_Reduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
//               MPI_Op op, int root, MPI_Comm comm)
//Deletable if recvbuf is no used
      Attributes["mpi_reduce_"] = replace_with_memcpy<0,1,2,3>;
//int MPI_Allreduce(const void *sendbuf, void *recvbuf, int count,
//                  MPI_Datatype datatype, MPI_Op op, MPI_Comm comm)
      Attributes["mpi_allreduce_"] = replace_with_memcpy<0,1,2,3>;
//int MPI_Bcast( void *buffer, int count, MPI_Datatype datatype, int root, 
//               MPI_Comm comm )
      Attributes["mpi_bcast_"] = DirectDelete;
//int MPI_Irecv(void *buf, int count, MPI_Datatype datatype, int source,
//              int tag, MPI_Comm comm, MPI_Request *request)
//int MPI_Isend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
//              MPI_Comm comm, MPI_Request *request)
      Attributes["mpi_isend_"] = DirectDelete;
      Attributes["mpi_irecv_"] = DirectDelete;
//Deletable if recvbuf is no used
//int MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
//             MPI_Comm comm)
//int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag,
//             MPI_Comm comm, MPI_Status *status)
      Attributes["mpi_send_"] = DirectDelete;
      Attributes["mpi_recv_"] = DirectDelete;
      Attributes["mpi_comm_dup_"] = DirectDelete;//bt
      // since alltoall need communite all threads, but we don't know the
      // content, so we could only copy send to recv
      Attributes["mpi_alltoall_"] = replace_with_memcpy<0,3,4,5>; //ft
   }else{
      Attributes["mpi_reduce_"] = mpi_nouse_recvbuf;
      Attributes["mpi_allreduce_"] = mpi_nouse_recvbuf;
      Attributes["mpi_bcast_"] = mpi_nouse_buf;
      Attributes["mpi_isend_"] = mpi_nouse_buf;
      Attributes["mpi_irecv_"] = mpi_nouse_buf;
      Attributes["mpi_send_"] = mpi_nouse_buf;
      Attributes["mpi_recv_"] = mpi_nouse_buf;
      Attributes["mpi_alltoall_"] = std::bind(nouse_at, _1, 3);
   }
//int MPI_Alltoall(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
//                 void *recvbuf, int recvcount, MPI_Datatype recvtype,
//                 MPI_Comm comm)
   Attributes["mpi_comm_split_"] = DirectDelete;
   Attributes["mpi_comm_rank_"] = std::bind(mpi_comm_replace, _1, &Protected, stat, "MPI_RANK");
   Attributes["mpi_comm_size_"] = std::bind(mpi_comm_replace, _1, &Protected, stat, "MPI_SIZE");
   //always delete mpi_wtime_
   Attributes["mpi_wtime_"] = DirectDeleteCascade;
   Attributes["mpi_error_string_"] = DirectDelete;
   Attributes["mpi_wait_"] = DirectDelete;
   Attributes["mpi_waitall_"] = DirectDelete;
   Attributes["mpi_barrier_"] = DirectDelete;
   Attributes["mpi_init_"] = mpi_direct_delete;
   Attributes["mpi_finalize_"] = mpi_direct_delete;
   Attributes["mpi_abort_"] = DirectDelete;
   Attributes["main"] = std::bind(direct_return, _1, AttributeFlags::None);
}

ReduceCode::~ReduceCode()
{
   delete CGF;
   delete ignore;
}

//==================================ATTRIBUTE END===============================//
