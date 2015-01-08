#ifndef SLASHSHRINK_H_H
#define SLASHSHRINK_H_H
#include <llvm/IR/Instruction.h>
#include <llvm/ADT/SmallSet.h>
#include <llvm/IR/Metadata.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/Pass.h>

#include <unordered_map>

#include "Resolver.h"
#include "Adaptive.h"

namespace lle {
   struct MarkPreserve;
   class SlashShrink;
   class ReduceCode;
   enum AttributeFlags {
      None = 0,
      IsDeletable = 1,
      IsPrint = IsDeletable,
      Cascade = 1<<1
   };
   inline AttributeFlags operator|(AttributeFlags a, AttributeFlags b)
   {return static_cast<AttributeFlags>(static_cast<int>(a) | static_cast<int>(b));}
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
   int ShrinkLevel;
   public:
   static char ID;
   SlashShrink();
   void getAnalysisUsage(llvm::AnalysisUsage& AU) const;

   bool runOnFunction(llvm::Function& F);
};

class lle::ReduceCode: public llvm::ModulePass
{
   typedef std::function<AttributeFlags(llvm::CallInst*)> Attribute_;
   std::unordered_map<std::string, Attribute_> Attributes;
   llvm::SmallSet<llvm::Function*, 4> ErasedFunc;
   llvm::CallGraphNode* root;
   llvm::DominatorTree* DomT;

   DSE_Adaptive dse;
   DAE_Adaptive dae;
   Adaptive ic, simpCFG;

   AttributeFlags getAttribute(llvm::CallInst*) const;
   void walkThroughCg(llvm::CallGraphNode*);
   void washFunction(llvm::Function* F);
   void deleteDeadCaller(llvm::Function* F);
   AttributeFlags noused_global(llvm::GlobalVariable* , llvm::Instruction* );
   public:
   static char ID;
   ReduceCode();
   void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
   bool runOnModule(llvm::Module& M) override;
   bool runOnFunction(llvm::Function& F);
};
#endif
