#include "ddg.h"
#include <llvm/IR/Instruction.h>

using namespace std;
using namespace lle;
using namespace llvm;


DDG::DDG(ResolvedListParam& r,UnsolvedList& u,ImplicityLinkType& c) {
   for(auto I : r){
      resolved.insert(DDGNode{I});
   }
   for(auto& N : resolved){
      auto found = c.find(N.first);
      Use* implicity = (found == c.end())?nullptr:found->second;
      if(implicity)
         N.second.push_back(static_cast<DDGNode*>(&*resolved.find(N.first)));
      else{
         Instruction* Inst = dyn_cast<llvm::Instruction>(N.first);
         if(Inst){
            for(auto O = Inst->op_begin(),E=Inst->op_end();O!=E;++O){
               N.second.push_back(static_cast<DDGNode*>(&*resolved.find(*O)));
            }
         }
      }
   }
   root = static_cast<DDGNode*>(&*resolved.begin());
   unsolved = u;
}
