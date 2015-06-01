#include "preheader.h"
#include "Resolver.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/ADT/DepthFirstIterator.h>
#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Analysis/CallGraph.h>

#include "ddg.h"
#include "util.h"
#include "debug.h"

using namespace std;
using namespace lle;
using namespace llvm;

char ResolverPass::ID = 0;
static RegisterPass<ResolverPass> Y("-Resolver","A Pass used to cache Resolver Result",false,false);
static const set<string> IgnoreFindCall = {
   "llvm.lifetime.end",
   "llvm.lifetime.start",
   "free"
};

Use* NoResolve::operator()(Value* V, ResolverBase* _UNUSED_)
{
   Instruction* I = dyn_cast<Instruction>(V);
   Assert(I,*I);
   if(!I) return NULL;
   if(isa<LoadInst>(I) || isa<StoreInst>(I))
      return &I->getOperandUse(0);
   //else if(isa<CallInst>(I))
   //   return NULL; // FIXME:not correct, first Assert, then watch which promot a call inst situation
   else
      Assert(0,*I);
   return NULL;
}

struct CallArgResolve{
   Use* operator()(Use* U){
      Argument* arg = findCallInstArgument(U); // adjust attribute
      if(arg && isArgumentWrite(arg) && arg->getNumUses()>0 && isa<StoreInst>(arg->user_back())) 
         // if no nocapture and readonly, it means could write into this addr
         // FIXME: when the last inst is not store, we consider this is unsolved
         // let it return NULL if failed, to let ResolverSet use next chain
         return U;
      else
         return NULL;
   }
};

Use* UseOnlyResolve::operator()(Value* V, ResolverBase* _UNUSED_)
{
   Use* Target = NULL, *Keep = NULL;
   if(LoadInst* LI = dyn_cast<LoadInst>(V)){
      Keep = Target = &LI->getOperandUse(0);
   }else if(CastInst* CI = dyn_cast<CastInst>(V)){
      Keep = Target = &CI->getOperandUse(0);
   }else if(GetElementPtrInst* GEP = dyn_cast<GetElementPtrInst>(V)) {
      Keep = Target = &GEP->getOperandUse(0); /* FIXME this is not good because
                                                GEP has many operands*/
   }else{
      Assert(0,*V);
      return NULL;
   }
   while( (Target = Target->getNext()) ){
      //seems all things who use target is after target
      auto U = Target->getUser();
      if(isa<StoreInst>(U) && U->getOperand(1) == Target->get())
         return &U->getOperandUse(0);
      else if(isa<CallInst>(U)){
         Use* R = CallArgResolve()(Target);
         if(R) return R;
      }
   }
   //if no use found, we consider this is a constant expr
   if(ConstantExpr* CE = dyn_cast<ConstantExpr>(Keep->get())){
      Instruction* TI = CE->getAsInstruction();
      return (*this)(TI, _UNUSED_);
   }

   return NULL;
}

Use* SLGResolve::operator()(Value *V, ResolverBase* _UNUSED_)
{
   if(LoadInst* LI = dyn_cast<LoadInst>(V)){
      llvm::Value* Ret = const_cast<llvm::Value*>(PI->getTrapedTarget(LI));
      if(Ret){
         StoreInst* SI = dyn_cast<StoreInst>(Ret);
         if(SI) return &SI->getOperandUse(0);
      }
   }
   return NULL;
}

/**
 * access whether a Instruction is reference global variable. 
 * if is, return the global variable
 * else return NULL
 */
static Use* access_global_variable(Instruction *I)
{
   Use* U = NULL;
	if(isa<LoadInst>(I) || isa<StoreInst>(I))
      U = &I->getOperandUse(0);
	else return NULL;
	while(ConstantExpr* CE = dyn_cast<ConstantExpr>(U->get())){
      Instruction*I = CE->getAsInstruction();
		if(isa<CastInst>(I))
         U = &I->getOperandUse(0);
		else break;
	}
	if(GetElementPtrInst* GEP = dyn_cast<GetElementPtrInst>(U->get()))
      U = &GEP->getOperandUse(GEP->getPointerOperandIndex());
	if(isa<GlobalVariable>(U->get())) return U;
	return NULL;
}

Use* SpecialResolve::operator()(Value *V, ResolverBase* RB)
{
   Use* Tg;
   if(auto LI = dyn_cast<LoadInst>(V)){
      Tg = &LI->getOperandUse(0);
   }else if(auto SI = dyn_cast<StoreInst>(V)){
      Tg = &SI->getOperandUse(0);
   }else return NULL;

   Value* U = Tg->get();
   if(auto GEP = dyn_cast<GetElementPtrInst>(U)){
      if(isRefGlobal(GEP->getPointerOperand()) &&
            GEP->getNumOperands()>2 &&
            !isa<Constant>(GEP->getOperand(1)))
         // GEP is load from a global variable array
         return Tg;
   } else if(auto Arg = dyn_cast<Argument>(U)){
      RB->ignore_cache(); /* since this doesn't point same callinst, we
                             shouldn't cache result */
      Function* F = Arg->getParent();
      CallInst* CI = RB->in_call(F);
      if(CI) return findCallInstParameter(Arg, CI);
   }
   return NULL;
}

/**
 * find where store to a global variable. 
 * require there a only store inst on this global variable.
 * NOTE!! this should replaced with UseOnlyResolve to provide a more correct
 * implement
 */
static void find_global_dependencies(const Value* GV,SmallVectorImpl<FindedDependenciesType>& Result)
{
	for(auto U = GV->user_begin(),E = GV->user_end(); U!=E;++U){
		Instruction* I = const_cast<Instruction*>(dyn_cast<Instruction>(*U));
		if(!I){
			find_global_dependencies(*U, Result);
			continue;
		}
		if(isa<StoreInst>(I)){
			Result.push_back(make_pair(MemDepResult::getDef(I),I->getParent()));
		}
	}
}

void MDAResolve::find_dependencies( Instruction* I, const Pass* P,
      SmallVectorImpl<FindedDependenciesType>& Result, NonLocalDepResult* NLDR)
{
   MemDepResult d;
   BasicBlock* SearchPos;
   Instruction* ScanPos;
   MemoryDependenceAnalysis& MDA = P->getAnalysis<MemoryDependenceAnalysis>();
   AliasAnalysis& AA = P->getAnalysis<AliasAnalysis>();

   if(Use* GV = access_global_variable(I)){
      find_global_dependencies(GV->get(), Result);
      return;
   }

   AliasAnalysis::Location Loc;
   if(LoadInst* LI = dyn_cast<LoadInst>(I)){
      Loc = AA.getLocation(LI);
   }else if(StoreInst* SI = dyn_cast<StoreInst>(I))
      Loc = AA.getLocation(SI);
   else
      assert(0);

   if(!NLDR){
      SearchPos = I->getParent();
      d = MDA.getPointerDependencyFrom(Loc, isa<LoadInst>(I), I, SearchPos, I);
      ScanPos = d.getInst();
   }else{
      ScanPos = NLDR->getResult().getInst();
      SearchPos = NLDR->getBB();
      d = NLDR->getResult();
      //we have already visit this BB;
      if(find_if(Result.begin(),Result.end(),[&SearchPos](FindedDependenciesType& f){
               return f.second == SearchPos;}
               ) != Result.end()){
         return;
      }
   }

   while(ScanPos){
      //if isDef, that's what we want
      //if isNonLocal, record BasicBlock to avoid visit twice
      if(d.isDef()||d.isNonLocal())
         Result.push_back(make_pair(d,SearchPos));
      if(d.isDef() && isa<StoreInst>(d.getInst()) )
         return;

      d = MDA.getPointerDependencyFrom(Loc, isa<LoadInst>(I), ScanPos, SearchPos, I);
      ScanPos = d.getInst();
   }

   //if local analysis result is nonLocal 
   //we didn't found a good result, so we continue search
   if(d.isNonLocal()){
      //we didn't record in last time
      Result.push_back(make_pair(d,SearchPos));

      SmallVector<NonLocalDepResult,32> NonLocals;

      MDA.getNonLocalPointerDependency(Loc, isa<LoadInst>(I), SearchPos, NonLocals);
      for(auto r : NonLocals){
         find_dependencies(I, P, Result, &r);
      }
   }else if(d.isNonFuncLocal()){
      //doing some thing here
      Resolver<NoResolve> R;
      bool has_argument = R.resolve_if(I->getOperand(0), [](Value* V){
               return isa<Argument>(V);
            });
      if(has_argument)
         Result.push_back(make_pair(d,SearchPos));
   }
}

Use* MDAResolve::operator()(llvm::Value * V, ResolverBase* _UNUSED_)
{
   Assert(pass,"Not initial with pass");

   SmallVector<FindedDependenciesType, 64> Result;
   Instruction* I = dyn_cast<Instruction>(V);

   if(!I) return NULL;

   find_dependencies(I, pass, Result);

   for(auto R : Result){
      Instruction* Ret = R.first.getInst();
      if(R.first.isDef() && Ret != I)
         return &R.first.getInst()->getOperandUse(0); //FIXME: 应该寻找使用的是哪个参数,而不是直接认为是0
      if(R.first.isNonFuncLocal())
         return NULL;
   }
   return NULL;
}

list<Value*> ResolverBase::direct_resolve( Value* V, unordered_set<Value*>& resolved, function<void(Value*)> lambda)
{
   std::list<llvm::Value*> unresolved;

   if(resolved.find(V) != resolved.end())
      return unresolved;
   if(isa<Argument>(V)){
      unresolved.push_back(V);
   } else if(isa<Constant>(V)){
      resolved.insert(V);
      lambda(V);
   }else if(Instruction* I = dyn_cast<Instruction>(V)){
      if(isa<LoadInst>(I)){
         unresolved.push_back(I);
      }else{
         resolved.insert(I);
         lambda(I);
         uint N = isa<StoreInst>(I)?1:I->getNumOperands();/*if isa store inst,
                                                         we only solve arg 0*/
         for(uint i=0;i<N;i++){
            Value* R = I->getOperand(i);
            auto rhs = direct_resolve(R, resolved, lambda);
            unresolved.insert(unresolved.end(), rhs.begin(), rhs.end());
         }
      }
   }
   return unresolved;
}

void ResolverBase::_empty_handler(llvm::Value *)
{
}

CallInst* ResolverBase::in_call(Function *F) const
{
   auto Ite = find_if(call_stack.begin(), call_stack.end(), [F](CallInst* CI){
         return CI->getCalledFunction() == F;
         });
   if(Ite!=call_stack.end()) return *Ite;
   CallInst* only;
   unsigned call_count = count_if(F->user_begin(), F->user_end(), [&only](User* U){
         return (only = dyn_cast<CallInst>(U)) > 0;
         });
   if(call_count==1) return only;
   return NULL;
}

ResolveResult ResolverBase::resolve(llvm::Value* V, std::function<void(Value*)> lambda)
{
   call_stack.clear(); // clear call_stack
   std::unordered_set<Value*> resolved;
   std::list<Value*> unsolved;
   std::unordered_map<Value*, Use*> partial;

   unsolved.push_back(NULL); /* always make sure Ite wouldn't point to end();
      因为在一开始的时候,如果没有这句,Ite指向end(),然后又在end后追加数据,造成
      Ite失效.  */

   auto Ite = unsolved.begin();
   Value* next = V; // wait to next resolved;

   while(next || *Ite != NULL){
      if(stop_resolve) break;
      if(next){
         list<Value*> mid = direct_resolve(next, resolved, lambda);
         unsolved.insert(--unsolved.end(),mid.begin(),mid.end()); 
            // insert before NULL, makes NULL always be tail
         next = NULL;
         if(*Ite == NULL) advance(Ite, -mid.size()); /* *Ite == NULL means *Ite points 
            tail, and we insert before it, we should put forware what we inserted */
         continue;
      }

      Use* res = NULL;
      Instruction* I = dyn_cast<Instruction>(*Ite);
      //FIXME: may be we should directly drop A to deep_resolve(A)
      if(!I){
         ++Ite;
         continue;
      }

      res = deep_resolve(I);
      if(!res || res->getUser()!= *Ite/* if returns self, it means unknow
                                         answer, we shouldn't insert partial */
            ){ 
         partial.insert(make_pair(*Ite,res)); 
         if(!res){
            ++Ite;
            continue;
         }
      }
      User* U = res->getUser();

      resolved.insert(I); // original is resolved;
      lambda(I);
      resolved.insert(U);
      lambda(U);
      Ite = unsolved.erase(Ite);

recursive:
      if(isa<StoreInst>(U) || isa<LoadInst>(U)){
         next = res->get();
      }else if(auto CI = dyn_cast<CallInst>(U)){
         if(CI->getCalledFunction() != I->getParent()->getParent()){ 
            // original Instruction isn't in the function called, means param
            // direction is out, that is, put a param to called function, and
            // write out result into it.
            Argument* arg = findCallInstArgument(res);
            if(arg){
               // arg == NULL, maybe it calls a library function
               partial.insert(make_pair(arg,&arg->user_back()->getOperandUse(0)));
               next = arg->user_back();
               call_stack.push_back(CI);
            }
         }else{
            // original Instruction is in same function called, means param
            // direction is in, that is, the outter put a param to called
            // function, and we(in function body) read the value and do caculate
            if(!isa<AllocaInst>(res->get())){
               next = res->get();
               continue;
            }

            while((res = res->getNext())){
               Use* R = NULL;
               User* Ur = res->getUser();
               if(isa<CallInst>(Ur)){
                  R = CallArgResolve()(res);
                  if(R){
                     U = Ur;
                     goto recursive; // goto above siatuation
                  }
               }else if(isa<LoadInst>(Ur) || isa<StoreInst>(Ur)){
                  next = Ur;
                  partial.insert(make_pair(res->get(),res));
                  break;
               }else
                  AssertRuntime(0, "");
            }
         }
      }else
         next = U;
   }

   unsolved.pop_back(); // remove last NULL
   return make_tuple(resolved, unsolved, partial);
}

bool ResolverBase::resolve_if(Value *V, function<bool (Value *)> lambda)
{
   stop_resolve = false;
   bool ret = false;

   resolve(V,[&lambda, &ret, this](Value* v){
            if(lambda(v)){
               this->stop_resolve = true;
               ret = true;
            }
         });
   stop_resolve = false;
   return ret;
}

void ResolverPass::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
   AU.setPreservesAll();
}

ResolverPass::~ResolverPass(){
   for(auto I : impls)
      delete I.second;
}

static bool implicity_rule(Value* I, DataDepGraph& G);

ResolveEngine::ResolveEngine()
{
   implicity_rule = ::implicity_rule;
   max_iteration = UINT32_MAX;
   Cache = NULL;
}

void ResolveEngine::do_solve(DataDepGraph& G, CallBack& C)
{
   if(G.getRootKey().is<Use*>()){
      // first time run rule, because some filter would directly refuse
      // query itself, which make results empty, so we didn't run filter
      // at first time.
      Use* un = G.popUnsolved();
      for(auto r = rules.rbegin(), e = rules.rend(); r!=e; ++r)
         if((*r)(un, G)) break;
   }
   while(Use* un = G.popUnsolved()){
      if(++iteration>max_iteration) break;
      bool jump = false;
      for(auto& f : filters)
         if((jump = f(un))) break;
      // if refused by filter, it is ignored. so we shouldn't use
      // callback
      if(!jump) jump = C(un); 
      if(jump){
         G.markIgnore(un); // if refused, this node is ignored.
         continue;
      }
      for(auto r = rules.rbegin(), e = rules.rend(); r!=e; ++r)
         if((*r)(un, G)) break;
   }
}

DataDepGraph ResolveEngine::resolve(QueryTy Q, CallBack C)
{
   Use* R = NULL;
   DataDepGraph G;
   G.isDependency(implicity_rule == ::implicity_rule);
   iteration = 0;
   if(Q.is<Use*>())
      G.addUnsolved(*Q.get<Use*>());
   else
      implicity_rule(Q.get<Value*>(), G);
   G.setRoot(Q);
   if(Cache->ask(Q, R)){
      for(auto& f : filters) f(R);
      C(R);
      return G;
   }else
      Cache->storeKey(Q);
   do_solve(G, C);
   return G;
}

struct find_visit
{
   llvm::Value*& ret;
   ResolveCache* C;
   find_visit(llvm::Value*& ret, ResolveCache* C = NULL):ret(ret), C(C) {}
   bool operator()(Use* U){
      User* Ur = U->getUser();
      if (isa<LoadInst>(Ur))
         C->storeValue(ret = Ur, U->getOperandNo());
      else if (CallInst* CI = dyn_cast<CallInst>(Ur)) {
         // call inst also is a kind of visit
         StringRef Name = castoff(CI->getCalledValue())->getName();
         if (auto I = dyn_cast<IntrinsicInst>(CI))
            Name = getName(I->getIntrinsicID());
         if (!IgnoreFindCall.count(Name)) // some call should ignore
            C->storeValue(ret = CI, U->getOperandNo());
      }

      // most of time, we just care whether it has visitor,
      // so if we found one, we can stop search
      return ret;
   }
};

ResolveEngine::CallBack ResolveEngine::exclude(QueryTy Q)
{
   User* Tg = (Q.is<Use*>()) ? Q.get<Use*>()->getUser() : dyn_cast<User>(Q.get<Value*>());
   return [Tg](Use* U){
      User* Ur = U->getUser();
      return Tg == Ur;
   };
}

ResolveEngine::CallBack ResolveEngine::findVisit(Value*& V)
{
   V = NULL;
   return ::find_visit(V, this->Cache);
}

Value* ResolveEngine::find_visit(QueryTy Q)
{
   addFilter(exclude(Q));
   Value* V = NULL;
   resolve(Q, ::find_visit(V));
   rmFilter(-1);
   return V;
}

ResolveEngine::CallBack ResolveEngine::findStore(Value *&V)
{
   V = NULL;
   return [&V](Use* U){
      User* Ur = U->getUser();
      if(isa<StoreInst>(Ur)){
         V=Ur;
         return true;
      }
      return false;
   };
}

ResolveEngine::CallBack ResolveEngine::findRef(Value *&V)
{
   auto visit = findVisit(V);
   auto store = findStore(V);
   return [visit, store](Use* U) {
      return visit(U)||store(U);
   };
}

//===========================RESOLVE RULES======================================//
static bool useonly_rule_(Use* U, DataDepGraph& G)
{
   User* V = U->getUser();
   Use* Tg = U;
   if(isa<LoadInst>(V))
      Tg = U; // if is load, find after use
   else if(isa<GetElementPtrInst>(V))
      Tg = &*V->use_begin(); // if is GEP, find all use @example : %5 = [GEP] ; store %6, %5
   else return false;

   do{
      //seems all things who use target is after target
      auto V = Tg->getUser();
      if(isa<StoreInst>(V) && V->getOperand(1) == Tg->get()){
         G.addSolved(U, V->getOperandUse(0));
         return true;
      }
   } while( (Tg = Tg->getNext()) );
   return false;
}
static bool direct_rule_(Use* U, DataDepGraph& G)
{
   Value* V = U->get();
   if(auto I = dyn_cast<Instruction>(V)){
      G.addSolved(U, I->op_begin(), I->op_end());
      return true;
   }else if(auto C = dyn_cast<Constant>(V)){
      G.addSolved(U, C);
      return true;
   }
   return false;
}
static bool direct_inverse_rule_(Use* U, DataDepGraph& G)
{
   Value* V = U->getUser();
   std::vector<Use*> uses;
   pushback_to(V->use_begin(), V->use_end(), uses);
   G.addSolved(U, uses.rbegin(), uses.rend());
   return true;
}
static bool use_inverse_rule_(Use* U, DataDepGraph& G)
{
   Value* V = U->get();
   std::vector<Use*> uses;
   auto bound = find_iterator(*U);
   pushback_to(V->use_begin(), bound, uses);
   G.addSolved(U, uses.rbegin(), uses.rend());
   // add all gep before inst, for dont' miss load like:
   // %0 = getelementptr
   // query
   // load %0
   for_each(bound, V->use_end(), [U, &G](Use& u) {
      auto GEP = isGEP(u.getUser());
      if(GEP && GEP->getPointerOperand() == u.get())
         G.addSolved(U,u);
   });
   return true;
}
static bool implicity_rule(Value* V, DataDepGraph& G)
{
   if(StoreInst* SI = dyn_cast<StoreInst>(V)){
      G.addSolved(SI, SI->getOperandUse(0));
      return false;
   }else if(User* I = dyn_cast<User>(V)){
      G.addSolved(I, I->op_begin(), I->op_end());
      return true;
   }else
      AssertRuntime(0, "a non-user couldn't have dependency");
}
static bool implicity_inverse_rule(Value* I, DataDepGraph& G)
{
   std::vector<Use*> uses;
   pushback_to(I->use_begin(), I->use_end(), uses);
   G.addSolved(I, uses.rbegin(), uses.rend());
   return true;
}
static bool gep_rule_(Use* U, DataDepGraph& G)
{
   // if isa GEP, means we already have solved before.
   if(isa<GetElementPtrInst>(U->getUser())) return false;
   Type* Ty = U->get()->getType();
   if(Ty->isPointerTy()) Ty = Ty->getPointerElementType();
   // only struct or array can have GEP
   if(!Ty->isStructTy() && !Ty->isArrayTy()) return false;
   Use* Tg = U;
   bool ret = false;
   while( (Tg = Tg->getNext()) ){
      auto GEP = isGEP(Tg->getUser());
      if(GEP && GEP->getPointerOperand() == Tg->get() ){
         // add all GEP to Unsolved
         G.addSolved(U, GEP->getOperandUse(0));
         ret = true;
      }
   }
   return ret;
}
static bool icast_rule_(Use* U, DataDepGraph& G)
{
   if(auto CI = dyn_cast<CastInst>(U->get()))
      G.addSolved(U, CI->getOperandUse(0));
   return false;// always want to find more result
}

bool InitRule::operator()(Use* U, DataDepGraph& G)
{
   if(!initialized){
      initialized = true;
      return rule(U,G);
   }
   return false;
}

#if 0
void MDARule::operator()(ResolveEngine &RE)
{
   MemoryDependenceAnalysis& MDA = this->MDA;
   AliasAnalysis&AA = this->AA;
   ResolveEngine::SolveRule S = [&MDA,&AA](Use* U, DDGraph& G){
      LoadInst* LI = dyn_cast<LoadInst>(U->getUser());
      if(LI==NULL) return false;
      MemDepResult Dep = MDA.getDependency(LI);
      //Def: 定义, 可能是Alloca, 或者...
      //Clobber: 改写, Load依赖的Store, Store改写了内存
      if(!Dep.isDef() && !Dep.isClobber()) return false;
      AliasAnalysis::Location Loc = AA.getLocation(LI);
      //inspired from DeadStoreElimination.cpp
      if(!Loc.Ptr) return false;
      while(Dep.isDef() || Dep.isClobber()){
         Instruction* DepWrite = Dep.getInst();
         Dep = MDA.getPointerDependencyFrom(Loc, 1, DepWrite, LI->getParent());
      }
   };
   RE.addRule(S);
}
#endif

void ResolveEngine::base_rule(ResolveEngine& RE)
{
   RE.addRule(SolveRule(direct_rule_));
   RE.implicity_rule = ::implicity_rule;
}
void ResolveEngine::ibase_rule(ResolveEngine& RE)
{
   RE.addRule(SolveRule(direct_inverse_rule_));
   RE.implicity_rule = ::implicity_inverse_rule;
}
const ResolveEngine::SolveRule ResolveEngine::useonly_rule = useonly_rule_;
const ResolveEngine::SolveRule ResolveEngine::gep_rule = gep_rule_;
const ResolveEngine::SolveRule ResolveEngine::iuse_rule = use_inverse_rule_;
const ResolveEngine::SolveRule ResolveEngine::icast_rule = icast_rule_;

//===============================RESOLVE RULES END===============================//
//=============================RESOLVE FILTERS BEGIN=============================//
GEPFilter::GEPFilter(GetElementPtrInst* GEP)
{
   if(GEP == NULL){
      idxs.clear();
      return;
   }
   idxs.reserve(GEP->getNumIndices());
   for(auto I=GEP->idx_begin(), E = GEP->idx_end(); I!=E; ++I){
      ConstantInt* CI = dyn_cast<ConstantInt>(I->get());
      if(CI) idxs.push_back(CI->getZExtValue());
      else break;
   }
}
bool GEPFilter::operator()(llvm::Use* U)
{
   if(idxs.empty()) return false; // if idxs empty, disable filter.
   User* Tg = U->getUser();
   if(ConstantExpr* CE = dyn_cast<ConstantExpr>(Tg))
      Tg = CE->getAsInstruction();
   if(auto Inst = dyn_cast<GetElementPtrInst>(Tg)){
      bool eq = (idxs.size() <= Inst->getNumIndices());
      eq = eq && std::equal(idxs.begin(), idxs.end(), Inst->idx_begin(), [](uint64_t L, Value* R){
            Constant* C = dyn_cast<Constant>(R);
            return (C && C->getUniqueInteger() == L);
            });
      return !eq;
   }
   return false;
}

/** CGFilter : 利用CallGraph信息, 求解两条指令的偏序关系, 即给出一条指令作为基
 * 准点, 查询其它指令是基于CG在其之前还是之后.
 * 例如: 
 *   MAIN
 *     ├─ setup_submatrix_info (c)
 *     ├─ randlc
 *     │   ├─ sprnvc
 *     │   │   ├─ randlc (a)
 *     │   │   └─ icnvrt
 *     ├─ makea
 *     │   ├─ vecset (b)
 * (a) 在 (b) 之前, (c) 在 (a) 和 (b) 之前
 * 求其偏序关系, 即是对其排序, 可以将其映射到正数序列上即可比较. 
 * 映射规则:
 * 1. 对所有的callgraphnode标记, 其长度表示范围从 [first, last)
 * 2. 预留出足够的空间, 映射那些夹杂在call之间的语句.
 * 3. 先按深度优先遍历, 记录 PathLen 和 OldPathLen, first = OldPathLen - PathLen + 2;
 * 4. 再按后序遍历, 该节点为空, last = first + 1; 否则 last = last_valid_child.last + 1
 * 以上就完成了所有callgraphnode的映射.
 * 5. 对于任意一条指令, 找到 Call1 < I < Call2
 *    indexof(I) = indexof(Call2)
 *    indexof(Call2) = Function(Call2).first - 1
 * 6. 如果有 forall Call < I. 表明I在函数的最后部分.
 *    indexof(I) = I所在的函数.last - 1
 * 7. 对于重复调用的或三角调用或递归调用, 只考虑第一次, 忽略之后的调用
 * 查询时, 求出两个待比较指令的index, 如果不等则比较其大小, 如果相等, 表明两条
 * 指令一定在同一函数中, 直接返回其偏序关系
 *
 * 具体的示例可以参考单元测试. 以便取得更加深刻的理解.
 */
#ifndef NDEBUG
void DebugCGFilter(CGFilter* F)
{
   for(auto& I : F->order_map){
      errs()<<I.first->getName()<<":["<<I.second.first<<","<<I.second.last<<")\n";
   }
}
#endif
static CallGraphNode* last_valid_child(CallGraphNode* N, set<Value*>& Only)
{
   using RIte = std::reverse_iterator<CallGraphNode::iterator>;
   auto found = find_if(RIte(N->end()), RIte(N->begin()), [&Only](RIte::value_type& P){
            auto F = P.second->getFunction();
            return Only.count(P.first) && F != NULL && !F->isDeclaration();
         });
   if(found == RIte(N->begin())) return NULL;
   else return found->second;
}

CGFilter::CGFilter(CallGraphNode* root_, Instruction* threshold_inst_): root(root_)
{
   using std::placeholders::_1;
   unsigned LastPathLen = 0;
   int i=-1; // for initial
   for(auto N = df_begin(root), E = df_end(root); N!=E; ++N){
      Function* F = N->getFunction();
      if(F && !F->isDeclaration()){

         if(N.getPathLength()>1){
            // first we go through tree, only stores which we visited
            CallGraphNode* Parent = N.getPath(N.getPathLength()-2);
            CallGraphNode* Current = *N;
            auto found = find_if(Parent->begin(), Parent->end(), [Current](CallGraphNode::iterator::value_type& V){
                  return V.second == Current;
                  });
            Only.insert(&*found->first);
         }

         // we caculate [first
         unsigned CurPathLen = N.getPathLength();
         i += LastPathLen - CurPathLen + 2;
         LastPathLen = CurPathLen;
         order_map[F] = {(unsigned)i, 0, *N}; // a function only store minimal idx
      }
   }
   // we caculate last)
   for(auto N = po_begin(root), E = po_end(root); N!=E; ++N){
      Function* F = N->getFunction();
      if(F==NULL || F->isDeclaration()) continue;

      Record& r = order_map[F];
      auto Clast = last_valid_child(*N, Only);
      if(N->empty() || Clast == NULL) r.last = r.first + 1;
      else r.last = order_map[Clast->getFunction()].last + 1;
   }
   threshold = 0;
   threshold_inst = NULL;
   threshold_f = NULL;
   update(threshold_inst_);
}

unsigned CGFilter::indexof(llvm::Instruction *I)
{
   Function* ParentF = I->getParent()->getParent();
   CallGraphNode* Parent = order_map[ParentF].second;
   // this function doesn't in call graph
   if(Parent==NULL) return UINT_MAX;
   if(Parent->empty()) return order_map[ParentF].first;

   Function* Fmatch = NULL;
   for(auto t = Parent->begin(), e = Parent->end(); t!=e; ++t){
      Function* F = t->second->getFunction();
      if(F==NULL || F->isDeclaration()) continue;
      if(t->first == NULL || Only.count(t->first)==0) continue;
      Instruction* call_inst = dyn_cast<Instruction>(&*t->first);
      // last call_inst < threshold_inst < next call_inst
      // then threshold should equal to last call_inst's order
      if(I == call_inst || std::less<Instruction>()(I, call_inst)){
         Fmatch = F;
         break;
      }
   }
   if(Fmatch == NULL)
      return order_map[ParentF].last - 1;
   else
      return order_map[Fmatch].first - 1;
}
void CGFilter::update(Instruction* threshold_inst_)
{
   if(threshold_inst_==NULL) return;
   threshold_inst = threshold_inst_;
   threshold_f = threshold_inst->getParent()->getParent();
   threshold = indexof(threshold_inst);
}
bool CGFilter::operator()(Use* U)
{
   Instruction* I = dyn_cast<Instruction>(U->getUser());
   if(I==NULL) return false;
   BasicBlock* B = I->getParent();
   if(B==NULL) return false;
   Function* F = B->getParent();
   if(F==NULL) return false;

   // quick lookup:
   // it's index > it's function's index > threshold
   unsigned order = order_map[F].first;
   if(order > threshold) return false;

   order = indexof(I);
   if(order == UINT_MAX) return true;
   if(order == threshold){
      AssertRuntime(threshold_f == F, "should be same function "<<order<<":"<<threshold);
      return std::less_equal<Instruction>()(I, threshold_inst);
   }
   return order < threshold;
}
bool iUseFilter::operator()(Use* U)
{
   if (pos == NULL) return false;
   if (isGEP(U->getUser())) return false;
   if (auto I = dyn_cast<Instruction>(U->getUser()))
      if(std::less<Instruction>()(I, pos)) return true;
   return false;
}
//==============================RESOLVE FILTERS END==============================//

void ResolveCache::storeValue(Value* V, unsigned op)
{
   if(this == NULL) return;
   Cache[StoredKey] = std::make_pair(WeakVH(V), op);
}
bool ResolveCache::ask(QueryTy Q, Use*& R)
{
   if(this == NULL) return false;
   const auto Found = Cache.find(Q);
   if(Found == Cache.end()) return false;
   Value* V = Found->second.first;
   User* U = V?dyn_cast<User>(V):NULL;
   if(U == NULL){
      Cache.erase(Found);
      return false;
   }
   R = &U->getOperandUse(Found->second.second);
   return true;
}
