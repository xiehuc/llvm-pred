#ifndef SLASHSHRINK_H_H
#define SLASHSHRINK_H_H

#include <llvm/ADT/SmallSet.h>
#include <unordered_map>
#include "Adaptive.h"
#include "Resolver.h"

namespace lle {
   class CGFilter;
   class LoopTripCount;
   class ReduceCode;
   class IgnoreList;
   class MPIStatistics;
   enum AttributeFlags {
      None = 0,
      IsDeletable = 1,
      IsPrint = IsDeletable,
      Cascade = 1<<1
   };
   inline AttributeFlags operator|(AttributeFlags a, AttributeFlags b)
   {return static_cast<AttributeFlags>(static_cast<int>(a) | static_cast<int>(b));}
   enum ConfigFlags
   {
      NO_ADJUST = 0,
      DISABLE_GEP_FILTER = 1<<0,
      DISABLE_STORE_INLOOP = 1<<1,
   };
};
namespace llvm {
   class Use;
};

// provide some mpi statistics information
class lle::MPIStatistics
{
  size_t ref_num;
  std::vector<std::function<void()> > _on_empty;
  public:
  MPIStatistics():ref_num(0) {}
  size_t count(){ return ref_num;}
  void ref(llvm::CallInst*);
  void unref(llvm::CallInst*);
  void onEmpty(std::function<void()>&& F) {
    _on_empty.push_back(F);
  }
};

class lle::ReduceCode: public llvm::ModulePass
{
   typedef std::function<AttributeFlags(llvm::CallInst*)> Attribute_;
   std::unordered_map<std::string, Attribute_> Attributes;
   llvm::SmallSet<llvm::StoreInst*, 4> Protected;
   llvm::DenseMap<llvm::Function*, bool> DirtyFunc;
   IgnoreList* ignore;
   MPIStatistics mpi_stats;

   llvm::DominatorTree* DomT;
   LoopTripCount* LTC;
   DSE_Adaptive dse;
   DAE_Adaptive dae;
   Adaptive ic, simpCFG;
   CGFilter* CGF;

   AttributeFlags getAttribute(llvm::CallInst*);
   AttributeFlags getAttribute(llvm::StoreInst*);
   void washFunction(llvm::Function* F);
   AttributeFlags noused_param(llvm::Argument*);
   llvm::Value* noused_global(llvm::GlobalVariable*, llvm::Instruction* pos,
                              llvm::GetElementPtrInst* GEP,
                              ResolveEngine::CallBack
                              = ResolveEngine::always_false);
   llvm::Value* noused(llvm::Use&);

   public:
   static char ID;
   ReduceCode();
   ~ReduceCode();
   void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
   bool doInitialization(llvm::Module& M) override;
   bool runOnModule(llvm::Module& M) override;
   bool runOnFunction(llvm::Function& F);
   bool doFinalization(llvm::Module &) override;

   AttributeFlags nousedOperator(llvm::Use&, llvm::Instruction* pos,
                                 ConfigFlags config = NO_ADJUST);
};
#endif
