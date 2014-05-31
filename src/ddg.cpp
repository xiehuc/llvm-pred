#include "ddg.h"
#include "util.h"
#include <llvm/IR/Instruction.h>

using namespace std;
using namespace lle;
using namespace llvm;

DDGValue DDGraph::make_value(Value *root, DDGNode::Flags flags)
{
   DDGNode n;
   n.flags_ = flags;
   return make_pair(root,n);
}

DDGraph::DDGraph(ResolveResult& RR,Value* root) 
{
   auto& r = get<0>(RR);
   auto& u = get<1>(RR);
   auto& c = get<2>(RR);
   for(auto I : r){
      this->insert(make_value(I,DDGNode::NORMAL));
   }
   for(auto I : u){
      this->insert(make_value(I,DDGNode::UNSOLVED));
   }
   for(auto& N : *this){
      auto found = c.find(N.first);
      DDGNode& node = N.second;
      if(node.flags() & DDGNode::UNSOLVED) continue;
      Use* implicity = (found == c.end())?nullptr:found->second;
      if(implicity){
         DDGValue& v = *this->find(implicity->getUser());
         DDGNode& to = v.second;
         node.impl().push_back(&v);
         node.flags_ = DDGNode::IMPLICITY;
         if(isa<CallInst>(implicity->getUser())){
            Argument* arg = findCallInstArgument(implicity);
            auto found = c.find(cast<Value>(arg));
            Use* link = (found==c.end())?nullptr:found->second;
            if(link){
               to.impl().push_back(&*this->find(link->getUser()));
               to.flags_ = DDGNode::IMPLICITY;
            }
         }
      }else{
         Instruction* Inst = dyn_cast<llvm::Instruction>(N.first);
         if(!Inst) continue;
         for(auto O = Inst->op_begin(),E=Inst->op_end();O!=E;++O){
            auto Target = this->find(*O);
            if(Target != this->end())
               node.impl().push_back(&*this->find(*O));
         }
      }
   }
   this->root = &*this->find(root);
}
