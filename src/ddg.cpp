#include "ddg.h"
#include "util.h"
#include <llvm/IR/Instruction.h>

using namespace std;
using namespace lle;
using namespace llvm;


DDGraph::DDGraph(ResolveResult& RR,Value* root) 
{
   auto& r = get<0>(RR);
   auto& u = get<1>(RR);
   auto& c = get<2>(RR);
   for(auto I : r){
      this->insert(DDGNode{I,DDGNode::NORMAL});
   }
   for(auto I : u){
      this->insert(DDGNode{I,DDGNode::UNSOLVED});
   }
   for(auto& N : *this){
      auto found = c.find(N.first);
      DDGNode* node = static_cast<DDGNode*>(&N);
      if(node->flags() & DDGNode::UNSOLVED) continue;
      Use* implicity = (found == c.end())?nullptr:found->second;
      if(implicity){
         DDGNode* to = static_cast<DDGNode*>(&*this->find(implicity->getUser()));
         node->edges().push_back(to);
         node->flags() = DDGNode::IMPLICITY;
         if(isa<CallInst>(implicity->getUser())){
            Argument* arg = findCallInstArgument(implicity);
            auto found = c.find(cast<Value>(arg));
            Use* link = (found==c.end())?nullptr:found->second;
            if(link){
               to->edges().push_back(static_cast<DDGNode*>(&*this->find(link->getUser())));
               to->flags() = DDGNode::IMPLICITY;
            }
         }
      }else{
         Instruction* Inst = dyn_cast<llvm::Instruction>(N.first);
         if(!Inst) continue;
         for(auto O = Inst->op_begin(),E=Inst->op_end();O!=E;++O){
            auto Target = this->find(*O);
            if(Target != this->end())
               node->edges().push_back(static_cast<DDGNode*>(&*this->find(*O)));
         }
      }
   }
   this->root = static_cast<DDGNode*>(&*this->find(root));
}
