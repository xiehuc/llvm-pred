#ifndef SLASHSHRINK_H_H
#define SLASHSHRINK_H_H
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Metadata.h>
namespace lle {
   class MarkPreserve;
};

class lle::MarkPreserve
{
   enum {
      MarkNode = 1050 // must  unique with other
   };
   static bool enabled();
   static void mark(llvm::Instruction* Inst);
   static bool is_marked(llvm::Instruction* Inst)
   {
      return Inst->getMetadata(MarkNode)->getOperand(0) != NULL;
   }
};
#endif
