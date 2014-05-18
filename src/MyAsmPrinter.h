#ifndef LLE_MYASMPRINTER_H_H
#define LLE_MYASMPRINTER_H_H

#include <llvm/CodeGen/AsmPrinter.h>

namespace lle {
   class MyAsmPrinter;
};

class lle::MyAsmPrinter : public llvm::AsmPrinter
{
   public:
   explicit MyAsmPrinter(llvm::TargetMachine &TM, llvm::MCStreamer &Streamer):
      AsmPrinter(TM,Streamer)
   { }
   const char *getPassName() const {
      return "My X86 Assembly / Object Emitter";
   }

   bool runOnMachineFunction(llvm::MachineFunction &MF) ;
};

#endif
