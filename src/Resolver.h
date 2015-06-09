#ifndef RESOLVER_H_H
#define RESOLVER_H_H

#include <list>
#include <tuple>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <llvm/Pass.h>
#include <llvm/IR/Instruction.h>
#include <llvm/ADT/PointerUnion.h>
#include <llvm/Analysis/MemoryDependenceAnalysis.h>

#include <ProfileInfo.h>

namespace llvm{
   class CallGraphNode;
};

namespace lle{
   class DataDepGraph;
   class ResolveEngine;
   class ResolveCache;
   struct InitRule;
   struct MDARule;
   struct GEPFilter;
   struct CGFilter;
   struct iUseFilter;
};

// a redesigned new resolve engine. 
// support Use* 
// support CallBack
class lle::ResolveEngine
{
   public:
   // if return true, means found a solve.
   typedef std::function<bool(llvm::Use*, DataDepGraph&)> SolveRule;
   typedef llvm::PointerUnion<llvm::Value*, llvm::Use*> QueryTy;
   // if return true, stop solve in current branch
   typedef std::function<bool(llvm::Use*)> CallBack;

   ResolveEngine();
   // add a rule in engine
   void addRule(SolveRule rule){
      rules.push_back(rule);
   }
   template<typename R>
   typename std::result_of<R(ResolveEngine&)>::type
   addRule(R rule){
      rule(*this);
   }

   // add filter in engine, filter is a helper callback, which used to cut
   // search path.
   void addFilter(CallBack filter){
      filters.push_back(filter);
   }

   void rmFilter(int idx){
      filters.erase(filters.begin()+idx%filters.size());
   };
   void clearFilters(){
      filters.clear();
   }

   // when use cache, you should always not trust resolve returned ddg
   // because when cache result, ddg return's empty
   void useCache(ResolveCache& C);
   void useCache(std::nullptr_t null) { Cache = NULL; }

   void setMaxIteration(size_t max) { max_iteration = max;}
   DataDepGraph resolve(QueryTy Q, CallBack C = always_false);

   static bool always_false(llvm::Use*) {
      return false;
   }
   // { normal version: these used for lookup it use who
   // a public rule used for solve ssa dependency
   static void base_rule(ResolveEngine&);
   // a public rule used for solve simple load.
   static const SolveRule useonly_rule;
   // a public rule used for expose gep instruction
   static const SolveRule gep_rule;
   static const SolveRule global_rule;
   // }
   // { reversed version: these used for lookup who use it.
   // a public rule used for lookup it's user's uses
   static void ibase_rule(ResolveEngine&);
   // use with InitRule, a public rule used for 
   static const SolveRule iuse_rule;
   static const SolveRule icast_rule;
   // }
   // {
   static CallBack exclude(QueryTy);
   // find a visit(load and call) for Q, this behave like this:
   // addFilter(exclude(Q))//ignore itself
   // resolve(Q, if_is_load_then_store_it);
   // rmFilter(exclude(Q))
   llvm::Value* find_visit(QueryTy Q);
   // update V if find a visit inst(load and call)
   CallBack findVisit(llvm::Value*& V);
   // update V if find a store inst
   CallBack findStore(llvm::Value*& V);
   // update V if find a visit or store inst
   CallBack findRef(llvm::Value*& V);
   // }
   private:
   bool (*implicity_rule)(llvm::Value*, DataDepGraph& G);
   void do_solve(DataDepGraph& G, CallBack& C);

   std::vector<SolveRule> rules;
   std::vector<CallBack> filters;
   size_t max_iteration, iteration;
   ResolveCache* Cache;
};

/* a ResolveCache used for speed up ResolveEngine resolve progress */
class lle::ResolveCache
{
   public:
   typedef llvm::Use* QueryTy;
   /** ask whether a Q has dependency R */
   bool ask(QueryTy Q, llvm::Use*& R);
   bool ask(QueryTy Q, llvm::Value*& V, unsigned& op);
   /** store key Q to later make an entry */
   void storeKey(QueryTy Q) {
      if(this == NULL || Q == NULL) return;
      StoredKey = Q;
   }
   /** store a value to make an entry with stored key before */
   void storeValue(llvm::Value* V, unsigned op) {
      if(this == NULL || V == NULL) return;
      Cache[StoredKey] = std::make_pair(llvm::WeakVH(V), op);
   }
   private:
   // a WeakVH is smart, when value delete, it auto set itself to NULL
   // unsigned is the op idx
   llvm::DenseMap<QueryTy, std::pair<llvm::WeakVH, unsigned> > Cache;
   QueryTy StoredKey;
};


/** make a rule only run once **/
struct lle::InitRule
{
   bool initialized;
   ResolveEngine::SolveRule rule;
   InitRule(const ResolveEngine::SolveRule r):initialized(false),rule(r) {}
   bool operator()(llvm::Use*, DataDepGraph& G);
   // clear state by hand after one resolve
   void clear(){ initialized = false;}
};

struct lle::MDARule
{
   llvm::MemoryDependenceAnalysis& MDA;
   llvm::AliasAnalysis& AA;
   MDARule(llvm::MemoryDependenceAnalysis& MD, 
         llvm::AliasAnalysis& A):
      MDA(MD),AA(A) {}
   void operator()(llvm::Use*, DataDepGraph& G);
};

/** gep filter can limit gep search path, need use with gep_rule.
 * if we found a gep, and it equals filter, then we continue search, else we
 * can ignore this branch. 
 */
struct lle::GEPFilter
{
   std::vector<uint64_t> idxs;
   // it would only store front constants to idxs.
   // @example getelementptr @gv, 0, 1, %2
   // idxs == {0,1}
   // @param : must be a GetElementPtr
   GEPFilter(llvm::User*);
   GEPFilter(llvm::ArrayRef<uint64_t> idx):
      idxs(idx.begin(), idx.end()) {}
   GEPFilter(std::initializer_list<uint64_t> idx): idxs(idx) {}
   // it would only compare front idxs 
   // @example idxs == {0,1}
   // getelementptr @gv, 0, 1, 2 ==> true
   // getelementptr @gv, 0, 2    ==> false
   // getelementptr @gv, 0       ==> false
   bool operator()(llvm::Use*);
};

struct lle::CGFilter
{
   struct Record {
      unsigned first, last; // range [first, last)
      llvm::CallGraphNode* second;
   };
   llvm::DenseMap<llvm::Function*, Record> order_map;
   std::set<llvm::Value*> Only; // ignore repeat call
   unsigned threshold;
   llvm::Instruction* threshold_inst;
   llvm::Function* threshold_f;
   llvm::CallGraphNode* root;
   CGFilter(llvm::CallGraphNode* main, llvm::Instruction* threshold=nullptr);
   unsigned indexof(llvm::Instruction*);
   void update(llvm::Instruction* threshold);
   bool count(llvm::Function* F) {
      return order_map.count(F);
   }
   bool operator()(llvm::Use*);
};

// a inverse use filter is a tool that only let instructions 'after' pass
// through, it use std::less<Instruction> to implement, so it only can
// used in function inner. this is used with iuse_rule which import gep
// 'before'. so this would ignore gep inst. 
// this is special for such cases like:
// %0 = getelementptr
// load %0 <- ban
// query
// load %0 <- pass
struct lle::iUseFilter
{
   llvm::Instruction* pos;
   iUseFilter(ResolveEngine::QueryTy Q){
      update(Q);
   }
   void update(ResolveEngine::QueryTy Q)
   {
      using V = llvm::Value*;
      using U = llvm::Use*;
      using I = llvm::Instruction;
      pos = Q.is<V>() ? llvm::dyn_cast<I>(Q.get<V>()) :
         llvm::dyn_cast<I>(Q.get<U>()->getUser());
      if (pos->getParent() == NULL) pos = NULL;
   }
   bool operator()(llvm::Use*);

};

#endif
