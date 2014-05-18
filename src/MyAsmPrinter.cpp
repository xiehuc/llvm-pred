#include <llvm/Support/TargetRegistry.h>

#include <llvm/CodeGen/MachineBasicBlock.h>
#include <llvm/CodeGen/MachineFunction.h>
#include <llvm/CodeGen/MachineInstr.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/BasicBlock.h>

#include "MyAsmPrinter.h"

using namespace lle;
using namespace llvm;

bool MyAsmPrinter::runOnMachineFunction(MachineFunction &MF)
{
   for(MachineFunction::iterator it=MF.begin();it!=MF.end();it++)
   {
      MachineBasicBlock &mbb = *it;
      const BasicBlock *bb = mbb.getBasicBlock();
      if(bb==NULL) {//outs() << "no basic block\n"; 
         continue;}
      //std::string bb_name;
      //bb_name = (bb->getName()).str();
      //outs() << bb_name << '\n';
      //MachineInstr &mi = mbb.front();
      //outs()  <<mi.getOpcode() << '\n';
      for(MachineBasicBlock::iterator MB_it=mbb.begin();MB_it!=mbb.end();MB_it++)
      {
         MachineInstr &mi = *MB_it;
         mi.print(errs(),&TM,false);
      }
   }
   return false;
}
