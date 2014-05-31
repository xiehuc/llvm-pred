#include "ddg.h"
#include "util.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/ADT/PostOrderIterator.h>

#include "debug.h"

using namespace std;
using namespace lle;
using namespace llvm;

static Twine to_expr(Value* V, DDGNode* N, int& ref_num);

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

static Twine to_expr(LoadInst* LI, DDGNode* N, int& R)
{
   if(N->flags() == DDGNode::UNSOLVED){
      raw_string_ostream O(N->expr_buf);
      pretty_print(LI, O);
      return O.str();
   }else{
      Assert(N->impl().size()==1,"");
      Assert(isa<StoreInst>(N->impl().front()->first),"");
      return N->impl().front()->second.expr().str();
   }
}

static Twine to_expr(Constant* C, DDGNode* N, int& R)
{
   if(isa<GlobalValue>(C))
      return "@"+C->getName();
   else{
      raw_string_ostream O(N->expr_buf);
      pretty_print(C,O);
      return O.str();
   }
}

static Twine to_expr(Value* V, DDGNode* N, int& ref_num)
{
   if(Constant* C = dyn_cast<Constant>(V))
      return to_expr(C,N,ref_num);
   else if(isa<Argument>(V))
      return "%"+V->getName();

   Instruction* I = dyn_cast<Instruction>(V);
   Assert(I,*I);

   if(auto BI = dyn_cast<BinaryOperator>(V)){
      Assert(N->impl().size()==2,"");// Twine doesn't support 3 param
      return N->expr_buf = (N->impl().front()->second.expr()+lookup_sym(BI).first+N->impl().back()->second.expr()).str();
   }else if(auto CI = dyn_cast<CmpInst>(V)){
      Assert(N->impl().size()==2,"");
      return N->impl().front()->second.expr()+lookup_sym(CI).first+N->impl().back()->second.expr();
   }else if(isa<StoreInst>(V)){
      Assert(N->impl().size()==1,"");
      return N->impl().front()->second.expr();
   }else if(auto LI = dyn_cast<LoadInst>(V))
      return to_expr(LI, N, ref_num);
   else if(isa<AllocaInst>(V))
      return "%"+V->getName();
   else
      Assert(0,*I);

   return "";
}

Twine DDGraph::expr()
{
   int ref_num = 0;
   for(auto I = po_begin(this), E = po_end(this); I!=E; ++I){
      I->second.expr_ = to_expr(I->first,&I->second,ref_num);
      errs()<<*(I->first)<<":"<<I->second.expr()<<"\n";
   }
   return root->second.expr();
}
