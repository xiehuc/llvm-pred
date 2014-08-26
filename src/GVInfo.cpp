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

bool GVInfo::findStoreOnGV(Value* V)
{
   for(auto U = V->use_begin(), E = V->use_end(); U!=E; ++U){
      bool found = false;
      if( auto CE = dyn_cast<ConstantExpr>(*U)){
         findStoreOnGV(CE);
         // don't record on ConstantExpr
         continue;
      }
      Instruction* Tg = dyn_cast<Instruction>(*U);
      if(!Tg) continue;
      if(auto SI = dyn_cast<StoreInst>(Tg)){
         if(SI->getPointerOperand() != V) continue;
         // store V, %another, isn't we need
         found = true;
         // Whether isa Array, record V->Tg
         if(isArray(SI->getValueOperand())){
            findStoreOnGV(SI->getValueOperand());
            // no need record twice
         }
      } else if( isa<CallInst>(Tg)){
         Argument* Arg = findCallInstArgument(&U.getUse());
         if(!Arg) continue;
         found = findStoreOnGV(Arg);
         // if Arg == NULL, maybe function is library function, which define
         // with void func(...);
      } else if( isa<CastInst>(Tg) || isa<GetElementPtrInst>(Tg)){
         found = findStoreOnGV(Tg);
      }
      if(found){
         Info.insert(std::make_pair(V, Data{Tg}));
         errs()<<*V<<"->"<<*Tg<<"\n";
         return true;
      }
   }
   return false;
}

bool GVInfo::runOnModule(Module& M)
{
   for(GlobalVariable& GV : M.getGlobalList()){
      findStoreOnGV(&GV);
   }
   return false;
}
