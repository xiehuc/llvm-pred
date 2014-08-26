#include "GVInfo.h"
#include "util.h"
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>

using namespace lle;
using namespace llvm;

char GVInfo::ID = 0;
static RegisterPass<GVInfo> X("GVinfo", "Provide Global Variable Information");

void GVInfo::getAnalysisUsage(AnalysisUsage & AU) const
{
   AU.setPreservesAll();
}

static GlobalVariable* find_all_param(Argument* Arg, bool& only)
{
   Function* F = Arg->getParent();
   GlobalVariable* param = NULL;
   unsigned count = 0;
   for(auto U = F->use_begin(),E = F->use_end(); U!=E; ++U){
      if(auto CI = dyn_cast<CallInst>(*U)){
         ++count;
         Value* P = findCallInstParameter(Arg, CI)->get();
         auto G = dyn_cast<GlobalVariable>(P);
         if(param!=NULL && param != G) break;
         param = G;
      }
   }
   only = count == 1;
   return param;
}

bool GVInfo::findStoreOnGV(Value* V, Constant* C)
{
   bool ret = false;
   for(auto U = V->use_begin(), E = V->use_end(); U!=E; ++U){
      bool found = false;
      if( auto CE = dyn_cast<ConstantExpr>(*U)){
         findStoreOnGV(CE, CE);
         // don't record on ConstantExpr
         continue;
      }
      Instruction* Tg = dyn_cast<Instruction>(*U);
      if(!Tg) continue;
      if(auto SI = dyn_cast<StoreInst>(Tg)){
         if(SI->getPointerOperand() != V) continue;
         // store V, %another, isn't we need
         found = true;
         info[C].store = SI;
         // Whether isa Array, record V->Tg
         if(isArray(SI->getValueOperand())){
            findStoreOnGV(SI->getValueOperand(), C);
            // no need record twice
         }
      } else if( isa<CallInst>(Tg)){
         Argument* Arg = findCallInstArgument(&U.getUse());
         if(!Arg) continue;
         /* write Argument Cache */
         bool write_once = false;
         auto V = find_all_param(Arg, write_once);
         if(V) arg_info.insert(std::make_pair(Arg, ArgInfo{write_once, V}));

         found = findStoreOnGV(Arg, C);
         // if Arg == NULL, maybe function is library function, which define
         // with void func(...);
      } else if( isa<CastInst>(Tg)){
         found = findStoreOnGV(Tg, C);
      } else if( isa<GetElementPtrInst>(Tg)){
         C = lookup(Tg)?:C;
         found = findStoreOnGV(Tg, C);
      }
      if(found){
         Data& D = info[C];
         D.resolve.push_front(Tg);
         ret = true;
      }
   }
   return ret;
}

bool GVInfo::runOnModule(Module& M)
{
   for(GlobalVariable& GV : M.getGlobalList()){
      findStoreOnGV(&GV, &GV);
   }
   return false;
}

Constant* GVInfo::lookup(Value *V)
{
   if(auto GEP = dyn_cast<GetElementPtrInst>(V)){
      if(auto Arg = dyn_cast<Argument>(GEP->getPointerOperand())){
         auto Ite = arg_info.find(Arg);
         if(Ite != arg_info.end() && Ite->second.param){
            GlobalVariable* G = Ite->second.param;
            SmallVector<Constant*, 4> Idx(GEP->getNumIndices());
            bool failed = false;
            std::transform(GEP->idx_begin(), GEP->idx_end(), Idx.begin(), [&failed](Value* V){
                  auto C = dyn_cast<Constant>(V);
                  if(!C) failed = true;
                  return C;
                  });
            if(!failed)
               return ConstantExpr::getGetElementPtr(G, Idx);
         }
      }
   }
   return dyn_cast<Constant>(V);
}

Value* GVInfo::getKey(Constant* C)
{
   auto found = info.find(C);
   if(found == info.end()) return NULL;
   return found->second.store->getPointerOperand();
}

Value* GVInfo::getValue(Constant* C)
{
   auto found = info.find(C);
   if(found == info.end()) return NULL;
   return found->second.store->getValueOperand();
}

void GVInfo::print(raw_ostream& OS, const Module* M) const
{
   for(auto& Pair : info){
      OS<<"Global Variable: "<<*Pair.first<<"\n";
      OS<<"  last store: "<<*Pair.second.store<<"\n";
      OS<<"  resolve: \n";
      for(auto I : Pair.second.resolve){
         OS<<*I<<"\n";
      }
      OS<<"\n";
   }
}

