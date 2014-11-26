#include "preheader.h"
#include "config.h"
#include "libtiming.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/GlobalVariable.h>

namespace lle{
class TimingSource{
   llvm::GlobalVariable* cpu_times;
   llvm::Type* EleTy;
   public:
   static llvm::StringRef getName(InstGroups IG);

   TimingSource():cpu_times(NULL), EleTy(NULL) {};
   void initArray(llvm::Module* M, llvm::Type* EleTy, bool force = false);
   InstGroups instGroup(llvm::Instruction* I) const throw(std::out_of_range);
   llvm::Value* createLoad(llvm::IRBuilder<>& Builder, InstGroups IG);
   void insert_load_source(llvm::Function& F);
   private:
};
}
