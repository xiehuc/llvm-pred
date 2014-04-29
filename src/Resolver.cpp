#include "Resolver.h"

#include <llvm/IR/Instructions.h>
#include <llvm/Support/raw_ostream.h>

using namespace std;
using namespace lle;
using namespace llvm;

Instruction* UseOnlyResolve::operator()(Value* V)
{
   Value* Target = NULL;
   if(LoadInst* LI = dyn_cast<LoadInst>(V)){
      Target = LI->getOperand(0);
   }
   for(auto U = Target->use_begin(), E = Target->use_end(); U != E; ++U){
   }

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
   bool changed = false;
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

      if(!isa<Instruction>(*Ite)){
         ++Ite;
         continue;
      }

      ++Ite;
   }
   auto& OS = outs();
   OS<<"::resolved list\n";
   for(auto i : resolved)
      OS<<"  "<<*i<<"\n";
   OS<<"::unresolved\n";
   for(auto i : unsolved)
      OS<<"  "<<*i<<"\n";
}

