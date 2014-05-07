#ifndef SLASHSHRINK_H_H
#define SLASHSHRINK_H_H
#include <llvm/IR/Instruction.h>
#include <llvm/ADT/SmallSet.h>
#include <llvm/IR/Metadata.h>
#include <llvm/Pass.h>

#include "Resolver.h"

namespace lle {
   struct MarkPreserve;
   class SlashShrink;
};

struct lle::MarkPreserve
{
   static llvm::StringRef MarkNode;
   static bool enabled();
   //mark a single instuction
   static void mark(llvm::Instruction* Inst, llvm::StringRef origin = "");
   //mark recusively instructions
   static std::list<llvm::Value*> mark_all(llvm::Value* V, ResolverBase& R, llvm::StringRef origin = "");
   //a helper to use mark_all with a resolver
   template<typename T>
   static std::list<llvm::Value*> mark_all(llvm::Value* V, llvm::StringRef origin = "");
   static bool is_marked(llvm::Instruction* Inst)
   {
      llvm::MDNode* MD = Inst->getMetadata(MarkNode);
      if(MD) return MD->getOperand(0) != NULL;
      return false;
   }
};

template<typename T>
std::list<llvm::Value*> 
lle::MarkPreserve::mark_all(llvm::Value* V, llvm::StringRef origin)
{
   lle::Resolver<T> R;
   return mark_all(V, R, origin);
}

/**
 * Slash and Shrink code to generate a mini core.
 * require mark the instructions need preserve
 * a environment SHRINK_LEVEL would control the shrink option
 * SHRINK_LEVEL : 0 --- do not write changes actually
 *                1 --- keep structure (default)
 */
class lle::SlashShrink: public llvm::FunctionPass
{
   llvm::SmallSet<std::string, 8> IgnoreFunc;
   public:
      static char ID;
      SlashShrink();
      void getAnalysisUsage(llvm::AnalysisUsage& AU) const;

      bool runOnFunction(llvm::Function& F);
};
#endif
