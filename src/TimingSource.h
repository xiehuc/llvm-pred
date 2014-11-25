#include "preheader.h"
#include "config.h"
#include "libtiming.h"
#include <llvm/ADT/SmallVector.h>

namespace lle{
class TimingSource{
   public:
   static llvm::StringRef getName(InstGroups IG);

   TimingSource() {};
   InstGroups instGroup(llvm::Instruction* I) const throw(std::out_of_range);
   //double getTiming(InstGroups IG);
   private:
};
}
