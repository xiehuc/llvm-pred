#include "TimingSource.h"
#include <llvm/Support/CommandLine.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/IR/Instruction.h>

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unordered_map>

#include "debug.h"

using namespace llvm;
using namespace lle;

static SmallVector<std::string,NumGroups> InstGroupNames;

StringRef TimingSource::getName(InstGroups IG)
{
   if(InstGroupNames.size() == 0){
      InstGroupNames.resize(NumGroups);
      auto& n = InstGroupNames;
      std::vector<std::pair<InstGroups,StringRef>> bits = {{Integer, "I"}, {I64, "I64"}, {Float,"F"}, {Double,"D"}}, 
         ops = {{Add,"Add"},{Mul, "Mul"}, {Div, "Div"}, {Mod, "Mod"}};
      for(auto bit : bits){
         for(auto op : ops)
            n[bit.first|op.first] = (bit.second+op.second).str();
      }
   }
   return InstGroupNames[IG];
}

#ifdef TIMING_SOURCE_lmbench

InstGroups TimingSource::instGroup(Instruction* I) const throw(std::out_of_range)
{
   Type* T = I->getType();
   unsigned Op = I->getOpcode();
   unsigned bit,op;
   auto e = std::out_of_range("no group for this instruction");

   if(T->isIntegerTy(32)) bit = Integer;
   else if(T->isIntegerTy(64)) bit = I64;
   else if(T->isFloatTy()) bit = Float;
   else if(T->isDoubleTy()) bit = Double;
   else throw e;

   switch(Op){
      case Instruction::Add: op = Add;break;
      case Instruction::Mul: op = Mul;break;
      /*case Instruction::Div: op = Div;break;
      case Instruction::Mod: op = Mod;break;*/
      default: throw e;
   }

   return static_cast<InstGroups>(bit|op);
}
#endif

