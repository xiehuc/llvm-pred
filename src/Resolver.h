#ifndef RESOLVER_H_H
#define RESOLVER_H_H

#include <list>
#include <tuple>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <llvm/Pass.h>
#include <llvm/IR/Instruction.h>
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

   struct DDGraph;
   class ResolveEngine;
   struct InitRule;
   struct MDARule;
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

   llvm::Value* find_store(llvm::Use& V);

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
   typedef std::function<bool(llvm::Use*, DDGraph&)> SolveRule;
   // if return true, stop solve in current branch
   typedef std::function<bool(llvm::Use*)> CallBack;

   private:
   static const CallBack always_false;
   static bool implicity_rule(llvm::Instruction*, DDGraph& G);
   std::vector<SolveRule> rules;
   void do_solve(DDGraph& G, CallBack& C);

   public:
   ResolveEngine() {}
   // add a rule in engine
   void addRule(SolveRule rule){
      rules.push_back(rule);
   }
   template<typename R>
   void addRule(R rule){
      rule(*this);
   }
   DDGraph resolve(llvm::Instruction* I, CallBack C = always_false);
   DDGraph resolve(llvm::Use& U, CallBack C = always_false);

   // { normal version: these used for lookup it use who
   // a public rule used for solve ssa dependency
   static const SolveRule base_rule;
   // a public rule used for solve simple load.
   static const SolveRule useonly_rule;
   // a public rule used for expose gep instruction
   static const SolveRule gep_rule;
   // }
   // { reversed version: these used for lookup who use it.
   // a public rule used for lookup it's user's uses
   static const SolveRule ibase_rule;
   // use with InitRule, a public rule used for 
   static const SolveRule iuse_rule;
   // }
   // return stored value if found
   llvm::Value* find_store(llvm::Use&, CallBack C = always_false);
   // return loadinst or callinst
   llvm::Value* find_visit(llvm::Use&, CallBack C = always_false);
};

/** make a rule only run once **/
struct lle::InitRule
{
   bool initialized;
   ResolveEngine::SolveRule rule;
   InitRule(const ResolveEngine::SolveRule r):initialized(false),rule(r) {}
   void operator()(ResolveEngine& RE);
};

struct lle::MDARule
{
   llvm::MemoryDependenceAnalysis& MDA;
   llvm::AliasAnalysis& AA;
   MDARule(llvm::MemoryDependenceAnalysis& MD, 
         llvm::AliasAnalysis& A):
      MDA(MD),AA(A) {}
   void operator()(ResolveEngine& RE);
};

#endif
