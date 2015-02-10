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
#include <llvm/Analysis/CallGraph.h>
#include <llvm/Analysis/MemoryDependenceAnalysis.h>

#include <ProfileInfo.h>

namespace lle{
   struct NoResolve;
   struct UseOnlyResolve;
   struct SpecialResolve;
   struct SLGResolve;
   class MDAResolve;

   class ResolverBase;
   template<typename Impl>
   class Resolver;
   class ResolverSet;
   class ResolverPass;

   typedef std::tuple<
      std::unordered_set<llvm::Value*>, // all resolved value
      std::list<llvm::Value*>, // all unresolved value
      std::unordered_map<llvm::Value*, llvm::Use*> // some part deep_resolve map
         >
      ResolveResult;
	typedef std::pair<llvm::MemDepResult,llvm::BasicBlock*> FindedDependenciesType;

   class DataDepGraph;
   class ResolveEngine;
   struct InitRule;
   struct MDARule;
   struct GEPFilter;
   struct CGFilter;
};

/**
 * provide a stright forward deep resolve
 */
struct lle::NoResolve
{
   llvm::Use* operator()(llvm::Value*, lle::ResolverBase*);
};

/**
 * Use llvm User and Use to provide deep resolve
 */
struct lle::UseOnlyResolve
{
   llvm::Use* operator()(llvm::Value*, lle::ResolverBase*);
};

/**
 * solve some special global variable situation
 * 1. if load a GetElementPtr and it depends on a global array variable. we
 *    consider this global variable is main datastructure, may write only once.
 *    and use anywhere. so we return it as a solve.
 * 2. if load a Argument, check whether there is a call argument's function before.
 *    if has. return this call inst's parameter
 */
struct lle::SpecialResolve
{
   llvm::Use* operator()(llvm::Value*, lle::ResolverBase*);
};

class lle::SLGResolve
{
   llvm::ProfileInfo* PI;
   public:
   SLGResolve(){PI = nullptr;}
   void initial(llvm::ProfileInfo* PI){this->PI = PI;}
   llvm::Use* operator()(llvm::Value*, lle::ResolverBase*);
};

/**
 * Use llvm MemoryDependencyAnalysis to provide deep resolve
 * useage : Resolver<MDAResolve> R;
 *          R.get_impl().initial(this); //<--- initial with Pass
 *          R.resolve(...);
 *
 * note   : this class is not implement well, add need rewrite
 */
class lle::MDAResolve
{
   llvm::Pass* pass;

   public:
   MDAResolve(){pass = NULL;}
   void initial(llvm::Pass* pass){ this->pass = pass;}
   llvm::Use* operator()(llvm::Value*, lle::ResolverBase*);

   /** use this to find a possible dep results.
    * which may clobber, def, nonlocal, nonfunclocal
    * and do further judgement
    */
   static void find_dependencies( llvm::Instruction* , 
         const llvm::Pass* ,
         llvm::SmallVectorImpl<lle::FindedDependenciesType>& ,
         llvm::NonLocalDepResult* NLDR = NULL);
};

class lle::ResolverBase
{
   friend class ResolverSet;
   template<typename T> friend class Resolver;
   bool stop_resolve = false; // stop resolve immediately
   static std::list<llvm::Value*> direct_resolve(
         llvm::Value* V, 
         std::unordered_set<llvm::Value*>& resolved,
         std::function<void(llvm::Value*)>
         );
   std::vector<llvm::CallInst*> call_stack;

   virtual llvm::Use* deep_resolve(llvm::Instruction* I) = 0;

   protected:
   ResolverBase* parent = NULL; // if this is belong to parent
   bool disable_cache = false;
   // hash map, provide quick search Value* ---> Value*
   std::unordered_map<llvm::Value*, llvm::Use*> PartCache;

   public:
   virtual ~ResolverBase(){};
   static void _empty_handler(llvm::Value*);

   // ignore cache once. call this in a Resolver
   void ignore_cache(){ disable_cache = true; }

   // return if call_stack has a related Function
   // or if there only one call on related Function
   llvm::CallInst* in_call(llvm::Function*) const;
   
   // walk through V's dependent tree and callback
   ResolveResult resolve(llvm::Value* V, std::function<void(llvm::Value*)> = _empty_handler);
   // walk though V's dependent tree and callback
   // if lambda return true, immediately stop resolve
   // and return true
   bool resolve_if(llvm::Value* V, std::function<bool(llvm::Value*)> lambda);

};

template<typename Impl = lle::NoResolve>
class lle::Resolver: public lle::ResolverBase
{
   Impl impl;
   llvm::Use* deep_resolve(llvm::Instruction* I)
   {
      llvm::Use* Ret = NULL;
      auto Ite = PartCache.find(I);
      if(Ite != PartCache.end())
         return Ite->second;
      Ret = impl(I, parent?:this);
      bool& disable_cache = (parent?:this)->disable_cache;
      if(!disable_cache)
         // don't make cache collapse, every resolver makes it's own cache
         PartCache.insert(std::make_pair(I, Ret)); 
      disable_cache = false;
      return Ret;
   }
   public:
   static char ID;
   Impl& get_impl(){ return impl;}
};

template<typename T>
char lle::Resolver<T>::ID = 0;

class lle::ResolverSet: public lle::ResolverBase
{
   std::vector<ResolverBase*> impls;
   public:
   ResolverSet(std::initializer_list<ResolverBase*> args):impls(args)
   { }

   llvm::Use* deep_resolve(llvm::Instruction* I)
   {
      llvm::Use* Ret = NULL;
      for(unsigned i=0;i<impls.size();i++){
         impls[i]->parent = this;
         Ret = impls[i]->deep_resolve(I);
         impls[i]->parent = NULL;
         if(Ret) return Ret;
      }
      return NULL;
   }
};

class lle::ResolverPass: public llvm::FunctionPass
{
   std::map<void*, ResolverBase*> impls;
   public:
   static char ID;
   explicit ResolverPass():FunctionPass(ID){
   }
   ~ResolverPass();
   template<typename T>
   lle::Resolver<T>& getResolver(){
      typedef lle::Resolver<T> R;
      return static_cast<R&>(impls[&R::ID]?*impls[&R::ID]:*(impls[&R::ID]=new R()));
   }
   template<typename... T>
   ResolverSet getResolverSet(){
      return ResolverSet({(static_cast<ResolverBase*>(&getResolver<T>()))...});
   }
   void getAnalysisUsage(llvm::AnalysisUsage& AU) const;
   bool runOnFunction(llvm::Function& F) {return false;}
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

   private:
   bool (*implicity_rule)(llvm::Value*, DataDepGraph& G);
   std::vector<SolveRule> rules;
   std::vector<CallBack> filters;
   size_t max_iteration, iteration;
   void do_solve(DataDepGraph& G, CallBack& C);

   public:
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

   void setMaxIteration(size_t max) { max_iteration = max;}
   DataDepGraph resolve(QueryTy Q, CallBack C = always_false);

   static bool always_false(llvm::Use*, QueryTy) {
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
   // }
   // {
   // update V if find a visit inst(load and call)
   static CallBack findVisit(llvm::Value*& V);
   // update V if find a store inst
   static CallBack findStore(llvm::Value*& V);
   // update V if find a visit or store inst
   static CallBack findRef(llvm::Value*& V);
   // }
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
   GEPFilter(llvm::GetElementPtrInst*);
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


#endif
