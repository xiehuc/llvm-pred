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

static const set<string> IgnoreFindCall = {
   "llvm.lifetime.end",
   "llvm.lifetime.start",
   "free"
};

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
   if(Use* U = Q.dyn_cast<Use*>()){
      G.addUnsolved(*U);
      if (Cache && Cache->ask(U, R)) { // only use cache in search Use*
         for(auto& f : filters) f(R);
         C(R);
         return G;
      }else
         Cache->storeKey(U);
   }else
      implicity_rule(Q.get<Value*>(), G);
   G.setRoot(Q);
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
      string NameRef;
      StringRef Name;
      if (isa<LoadInst>(Ur))
         C->storeValue(ret = Ur, U->getOperandNo());
      else if (CallInst* CI = dyn_cast<CallInst>(Ur)) {
         // call inst also is a kind of visit
         StringRef Name = castoff(CI->getCalledValue())->getName();
         if (auto I = dyn_cast<IntrinsicInst>(CI))
            Name = NameRef = getName(I->getIntrinsicID());
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

void ResolveEngine::useCache(ResolveCache& C) {
#ifdef USE_CACHE
  Cache = &C;
#endif
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
      if(GEP && GEP->getOperand(0) == u.get())
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
      if(GEP && GEP->getOperand(0) == Tg->get() ){
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
GEPFilter::GEPFilter(User* GEP)
{
   if(GEP == NULL){
      idxs.clear();
      return;
   }
   assert(isGEP(GEP));
   idxs.reserve(GEP->getNumOperands()-1);
   for(auto I=GEP->op_begin()+1, E = GEP->op_end(); I!=E; ++I){
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

bool ResolveCache::ask(QueryTy Q, Use*& R)
{
   Value* V;
   unsigned op;
   if (!ask(Q, V, op))
      return false;
   User* U = dyn_cast<User>(V);
   if (U == NULL)
      return false;
   R = &U->getOperandUse(op);
   return true;
}

bool ResolveCache::ask(QueryTy Q, Value*& V, unsigned& op)
{
   const auto Found = Cache.find(Q);
   if(Found == Cache.end()) return false;
   V = Found->second.first;
   op = Found->second.second;
   if(V == NULL) Cache.erase(Found);
   return V!=NULL;
}
