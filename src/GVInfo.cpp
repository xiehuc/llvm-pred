#include "preheader.h"
#include "GVInfo.h"
#include "Resolver.h"
#include "util.h"
#include "KnownLibCallInfo.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>

#include "debug.h"

using namespace lle;
using namespace llvm;

char GVInfo::ID = 0;
static RegisterPass<GVInfo> X("GVinfo", "Provide Global Variable Information");
static LibCallFromFile LCFF;

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

bool GVInfo::findLoadOnGVPointer(Value* V, Constant* C)
{
   bool ret = false;
   for(auto U = V->use_begin(), E = V->use_end(); U!=E; ++U){
      bool found = false;
      Instruction* Tg = dyn_cast<Instruction>(*U);
      if(!Tg) continue;
      if(isa<LoadInst>(Tg))
         found = findStoreOnGV(Tg, C);
      if(found){
         Data& D = info[C];
         D.resolve.push_front(Tg);
         ret = true;
      }
   }
   return ret;
}

bool GVInfo::findStoreOnGV(Value* V, Constant* C)
{
   bool ret = false;
   for(auto U = user_begin(V), E = user_end(V); U!=E; ++U){
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
         Data& D = info[C];
         D.write_once &= !(D.store && D.store->getUser()!=SI);
         D.store = &SI->getOperandUse(0);
         // Whether isa Array, record V->Tg
         if(isArray(SI->getValueOperand())){
            // no need record twice
            if(findStoreOnGV(SI->getValueOperand(), C)==false){
               // if no found store on value, maybe there is a indirect store:
               //   % = load i8** key;
               //   store result, %
               findLoadOnGVPointer(SI->getPointerOperand(), C);
            }
         }
      } else if(auto CI = dyn_cast<CallInst>(Tg)){
         Argument* Arg = findCallInstArgument(&U.getUse());
         if(!Arg){
            // if Arg == NULL, maybe function is library function, which define
            // with void func(...);
            Function* CF = dyn_cast<Function>(castoff(CI->getCalledValue()));
            if(!CF) continue;
            auto FI = LCFF.getFunctionInfo(CF);
            if(FI && FI->UniversalBehavior & AliasAnalysis::ModRefResult::Mod){
               found = true;
               Data& D = info[C];
               D.write_once &= !(D.store && D.store->getUser()!=CI);
               D.store = &U.getUse();
            }
         }else{
            bool write_once = false;
            auto V = find_all_param(Arg, write_once);
            /* write Argument Cache */
            if(V) arg_info.insert(std::make_pair(Arg, ArgInfo{write_once, V}));

            found = findStoreOnGV(Arg, C);
         }
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

Constant* GVInfo::lookup(Value *V) const
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

const Value* GVInfo::getKey(Constant *C) const 
{
   auto found = info.find(C);
   if(found == info.end()) return NULL;
   if(!found->second.write_once) return NULL;
   auto I = found->second.store->getUser();

   if(auto CI = dyn_cast<CallInst>(I))
      return castoff(CI->getCalledValue());
   else if(auto SI = dyn_cast<StoreInst>(I))
      return SI->getPointerOperand();
   else
      return NULL;
}


void GVInfo::print(raw_ostream& OS, const Module* M) const
{
   for(auto& Pair : info){
      OS<<"Global Variable: "<<*Pair.first<<"\n";
      OS<<"  last store: "<<*Pair.second.store<<"\n";
      OS<<"  write once: "<<Pair.second.write_once<<"\n";
      OS<<"  resolve: \n";
      for(auto I : Pair.second.resolve){
         OS<<*I<<"\n";
      }
      OS<<"\n";
   }
}

Use* GVResolve::operator()(Value* V, ResolverBase*)
{
   Value* Tg = NULL;
   Assert(gv_info, "need initial with GVInfo first");
   if(auto LI = dyn_cast<LoadInst>(V)){
      Tg = LI->getPointerOperand();
   }
   if(!Tg) return NULL;
   Constant* CTg = gv_info->lookup(Tg);
   if(!CTg) return NULL;
   return const_cast<Use*>(gv_info->getValue(CTg));
}
