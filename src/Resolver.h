#ifndef RESOLVER_H_H
#define RESOLVER_H_H

#include <list>
#include <tuple>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <llvm/IR/Instruction.h>

namespace lle{
   struct NoResolve;
   struct UseOnlyResolve;
   struct MDAResolve;
   class ResolverBase;
   template<typename Impl>
   class Resolver;
   typedef std::tuple<
      std::unordered_set<llvm::Value*>, // all resolved value
      std::list<llvm::Value*>, // all unresolved value
      std::unordered_map<llvm::Value*, llvm::Instruction*> // some part deep_resolve map
         >
      ResolveResult;
};

/**
 * Doesn't provide deep resolve
 */
struct lle::NoResolve
{
   llvm::Instruction* operator()(llvm::Value*){ return NULL;}
};

/**
 * Use llvm User and Use to provide deep resolve
 */
struct lle::UseOnlyResolve
{
   llvm::Instruction* operator()(llvm::Value*);
};

/**
 * Use llvm MemoryDependencyAnalysis to provide deep resolve
 */
struct lle::MDAResolve
{
};

class lle::ResolverBase
{
   static std::list<llvm::Value*> direct_resolve(
         llvm::Value* V, 
         std::unordered_set<llvm::Value*>& resolved,
         std::function<void(llvm::Value*)>
         );

   virtual llvm::Instruction* deep_resolve(llvm::Instruction* I) = 0;

   protected:
   // hash map, provide quick search Value* ---> Value*
   std::unordered_map<llvm::Value*, llvm::Instruction*> PartCache;

   public:
      ResolveResult resolve(llvm::Value* V, std::function<void(llvm::Value*)>);
};

template<typename Impl = lle::NoResolve>
class lle::Resolver: public lle::ResolverBase
{
   Impl impl;
   llvm::Instruction* deep_resolve(llvm::Instruction* I)
   {
      llvm::Instruction* Ret = NULL;
      auto Ite = PartCache.find(I);
      if(Ite != PartCache.end())
         return Ite->second;
      Ret = impl(I);
      PartCache.insert(std::make_pair(I, Ret));
      return Ret;
   }
};



#endif
