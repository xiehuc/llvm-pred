#include "ddg.h"
#include <llvm/IR/Instruction.h>

using namespace std;
using namespace lle;
using namespace llvm;


DDGraph::DDGraph(ResolvedListParam& r,UnsolvedList& u,ImplicityLinkType& c,Value* root) 
{
   for(auto I : r){
      this->insert(DDGNode{I});
   }
   for(auto& N : *this){
      auto found = c.find(N.first);
      Use* implicity = (found == c.end())?nullptr:found->second;
      if(implicity)
         N.second.push_back(static_cast<DDGNode*>(&*this->find(N.first)));
      else{
         Instruction* Inst = dyn_cast<llvm::Instruction>(N.first);
         if(Inst){
            for(auto O = Inst->op_begin(),E=Inst->op_end();O!=E;++O){
               N.second.push_back(static_cast<DDGNode*>(&*this->find(*O)));
            }
         }
      }
   }
   this->root = static_cast<DDGNode*>(&*this->find(root));
   unsolved = u;
}
