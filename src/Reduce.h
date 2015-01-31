#ifndef SLASHSHRINK_H_H
#define SLASHSHRINK_H_H

#include <llvm/ADT/SmallSet.h>
#include <unordered_map>
#include "Adaptive.h"

namespace lle {
   class CGFilter;
   class LoopTripCount;
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

class lle::ReduceCode: public llvm::ModulePass
{
   typedef std::function<AttributeFlags(llvm::CallInst*)> Attribute_;
   std::unordered_map<std::string, Attribute_> Attributes;
   llvm::SmallSet<llvm::StoreInst*, 4> Protected;
   llvm::SmallSet<llvm::Function*, 4> ErasedFunc;
   llvm::CallGraphNode* root;
   llvm::DominatorTree* DomT;
   LoopTripCount* LTC;

   DSE_Adaptive dse;
   DAE_Adaptive dae;
   Adaptive ic, simpCFG;
   CGFilter* CGF;

   AttributeFlags getAttribute(llvm::CallInst*);
   AttributeFlags getAttribute(llvm::StoreInst*);
   void walkThroughCg(llvm::CallGraphNode*);
   void washFunction(llvm::Function* F);
   void deleteDeadCaller(llvm::Function* F);
   AttributeFlags noused_param(llvm::Argument*);
   AttributeFlags noused_global(llvm::GlobalVariable* , llvm::Instruction* );
   public:
   static char ID;
   ReduceCode();
   ~ReduceCode();
   void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
   bool runOnModule(llvm::Module& M) override;
   bool runOnFunction(llvm::Function& F);
};
#endif
