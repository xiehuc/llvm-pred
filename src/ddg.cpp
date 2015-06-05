#include "preheader.h"
#include "ddg.h"
#include "util.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/ADT/PostOrderIterator.h>

#include "debug.h"

using namespace std;
using namespace lle;
using namespace llvm;

cl::opt<bool> lle::Ddg("Ddg", cl::desc("Draw Data Dependencies Graph"));

//=======================NEW API==================================
void DataDepGraph::addUnsolved(llvm::Use *beg, llvm::Use *end)
{
   pushback_to(beg, end, unsolved);
}

void DataDepGraph::addSolved(DDGraphKeyTy K, Use* F, Use* T)
{
   std::vector<DDGraphKeyTy> V;
   V.reserve(T-F);
   for(auto U = F; U!=T; ++U){
      auto Found = &this->FindAndConstruct(U);
      Found->second.parent_ = this;
      V.push_back(Found->first);
      // shouldn't push direct to N, FindAndConstruct would adjust buckets
      if(Found->second.flags() == DataDepNode::Flags::UNSOLVED) 
         unsolved.push_back(U);
   }
   auto& N = (*this)[K];
   N.flags_ = DataDepNode::Flags::SOLVED;
   N.parent_ = this;
   N.impl().insert(N.impl().end(), V.begin(), V.end());
}

void DataDepGraph::addSolved(DDGraphKeyTy K, Value* C)
{
   auto Found = &this->FindAndConstruct(C);
   Found->second.flags_ = DataDepNode::Flags::SOLVED;

   auto& N = (*this)[K];
   N.flags_ = DataDepNode::Flags::SOLVED;
   N.parent_ = Found->second.parent_ = this;
   N.impl().push_back(C);
}

string llvm::DOTGraphTraits<DataDepGraph*>::getNodeLabel(Self::value_type* N, Self* G)
{
   std::string ret;
   llvm::raw_string_ostream os(ret);
   if(Use* U = N->first.dyn_cast<Use*>()){
      U->getUser()->print(os);
   }else{
      Value* V = N->first.get<Value*>();
      V->print(os);
      if(auto CI = dyn_cast<CallInst>(V)){
         Function* F = CI->getCalledFunction();
         if(!F) return ret;
         os<<"\n\t Argument Names: [ ";
         for(auto& Arg : F->getArgumentList())
            os<<Arg.getName()<<"  ";
         os<<"]";
      }
   }
   return ret.substr(ret.find_first_not_of(' '));
}
