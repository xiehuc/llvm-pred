#ifndef SLASHSHRINK_H_H
#define SLASHSHRINK_H_H
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Metadata.h>
namespace lle {
   struct MarkPreserve;
};

struct lle::MarkPreserve
{
   static llvm::StringRef MarkNode;
   static bool enabled();
   //mark a single instuction
   static void mark(llvm::Instruction* Inst);
   //mark recusively instructions
   static void mark_all(llvm::Value* V);
   static bool is_marked(llvm::Instruction* Inst)
   {
      llvm::MDNode* MD = Inst->getMetadata(MarkNode);
      if(MD) return MD->getOperand(0) != NULL;
      return false;
   }
};
#endif
