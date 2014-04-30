#ifndef RESOLVER_H_H
#define RESOLVER_H_H

#include <list>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <llvm/IR/Instruction.h>

namespace lle{
   struct UseOnlyResolve;
   struct MDAResolve;
   struct ResolveResult;
   class Resolver;
};

struct ResolveResult
{
   std::vector<llvm::Value*> resolved;
   std::list<llvm::Value*> unresolved;
};

struct lle::UseOnlyResolve
{
   llvm::Instruction* operator()(llvm::Value*);
};

struct lle::MDAResolve
{
};

class lle::Resolver
{

   // hash map, provide quick search Value* ---> Value*
   std::unordered_map<llvm::Value*, llvm::Instruction*> PartCache;

   llvm::Instruction* deep_resolve(llvm::Instruction* I)
   {
      llvm::Instruction* Ret = NULL;
      auto Ite = PartCache.find(I);
      if(Ite != PartCache.end())
         return Ite->second;
      Ret = UseOnlyResolve()(I);
      //Ret = impl.resolve(I);
      PartCache.insert(std::make_pair(I, Ret));
      return Ret;
   }

   static std::list<llvm::Value*> direct_resolve(
         llvm::Value* V, std::unordered_set<llvm::Value*>& resolved);
   public:
      Resolver() = default;

      void resolve(llvm::Value* V);
};

#endif
