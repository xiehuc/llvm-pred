#include "Resolver.h"

#include <llvm/IR/Instructions.h>
#include <llvm/Support/raw_ostream.h>

using namespace std;
using namespace lle;
using namespace llvm;

Instruction* UseOnlyResolve::operator()(Value* V)
{
   Use* Target = NULL;
   if(LoadInst* LI = dyn_cast<LoadInst>(V)){
      Target = &LI->getOperandUse(0);
   }else{
      outs()<<__LINE__<<*V<<"\n";
      return NULL;
   }
   while( (Target = Target->getNext()) ){
      //seems all things who use target is after target
      if(isa<StoreInst>(Target->getUser())) return dyn_cast<Instruction>(Target->getUser());
   }
#if 0
   for(auto U = Target->get()->use_begin(), E = Target->get()->use_end(); U != E; ++U){
      /*if(!isa<StoreInst>(*U) && !isa<CallInst>(*U))
         continue;
      Instruction* I = dyn_cast<Instruction>(*U);
      if(I->getParent()->getParend() != VI-*/
      outs()<<"xxx"<<**U<<"\n";
      //if(&U.getUse() == Target) break;

   }
   assert(0);
#endif

   return NULL;
}


list<Value*> Resolver::direct_resolve( Value* V, unordered_set<Value*>& resolved)
{
   std::list<llvm::Value*> unresolved;
   if(resolved.find(V) != resolved.end())
      return unresolved;
   if(llvm::isa<Argument>(V))
      unresolved.push_back(V);
   if(isa<Constant>(V))
      resolved.insert(V);
   if(Instruction* I = dyn_cast<Instruction>(V)){
      if(isa<LoadInst>(I) || isa<StoreInst>(I) || isa<CallInst>(I)){
         unresolved.push_back(I);
      }else{
         resolved.insert(I);
         for(unsigned int i=0;i<I->getNumOperands();i++){
            Value* R = I->getOperand(i);
            auto rhs = direct_resolve(R, resolved);
            unresolved.insert(unresolved.end(), rhs.begin(), rhs.end());
         }
      }
   }
   return unresolved;
}


void Resolver::resolve(llvm::Value* V)
{
   using namespace llvm;
   //bool changed = false;
   std::unordered_set<Value*> resolved;
   std::list<Value*> unsolved;

   unsolved.push_back(NULL); // always make sure Ite wouldn't point to end();

   auto Ite = unsolved.begin();
   Value* next = V; // wait to next resolved;

   while(next || *Ite != NULL){
      if(next){
         list<Value*> mid = direct_resolve(next, resolved);
         unsolved.insert(--unsolved.end(),mid.begin(),mid.end()); 
            // insert before NULL, makes NULL always be tail
         next = NULL;
         if(*Ite == NULL) advance(Ite, -mid.size()); // *Ite == NULL means *Ite points 
            //tail, and we insert before it, we should put forware what we
            //inserted 
         continue;
      }

      Instruction* I = dyn_cast<Instruction>(*Ite);
      if(!I){
         ++Ite;
         continue;
      }

      outs()<<__LINE__<<*I<<"\n";
      Instruction* res = deep_resolve(I);
      if(!res){
         ++Ite;
         continue;
      }

      resolved.insert(*Ite);
      Ite = unsolved.erase(Ite);
      if(isa<StoreInst>(res)){
         resolved.insert(res);
         next = res->getOperand(0);
      }else
         next = res;
   }
   auto& OS = outs();
   OS<<"::resolved list\n";
   for(auto i : resolved)
      OS<<"  "<<*i<<"\n";
   OS<<"::unresolved\n";
   for(auto i : unsolved)
      OS<<"  "<<*i<<"\n";
}

