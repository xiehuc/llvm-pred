#include "Resolver.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/raw_ostream.h>

#include "util.h"
#include "debug.h"

using namespace std;
using namespace lle;
using namespace llvm;

char ResolverPass::ID = 0;
static RegisterPass<ResolverPass> Y("-","A Pass used to cache Resolver Result",false,false);

static Argument* findCallInstArgument(CallInst* CI,Value* v)
{
   uint idx = find(CI->op_begin(), CI->op_end(), 
         v) - CI->op_begin(); //find argument position FIXME 这里有问题
   Assert(idx != CI->getNumOperands(), "");
   Function* CF = dyn_cast<Function>(castoff(CI->getCalledValue()));
   Assert(CF," called function should not be null");
   if(CF->getArgumentList().size() <= idx) // there are no enough argument
      // FIXME: 通常，调用的外部函数是(...)格式，也就是说不知道会不会写入！
      return NULL;
   auto ite = CF->getArgumentList().begin();
   advance(ite,idx); /* get function argument */
   return &*ite;
}

#ifdef ENABLE_DEBUG
static void debug_print_resolved(unordered_set<Value*>& resolved)
{
   for ( auto V : resolved){
      outs()<<*V<<"\n";
   }
}
#endif

Use* NoResolve::operator()(Value* V)
{
   Instruction* I = dyn_cast<Instruction>(V);
   Assert(I,*I);
   if(!I) return NULL;
   if(isa<LoadInst>(I) || isa<StoreInst>(I))
      return &I->getOperandUse(0);
      //return dyn_cast<Instruction>(I->getOperand(0));
   else if(isa<CallInst>(I))
      return NULL; // FIXME:not correct
   else
      Assert(0,*I);
   return NULL;
}

Use* UseOnlyResolve::operator()(Value* V)
{
   Use* Target = NULL, *Keep = NULL;
   if(LoadInst* LI = dyn_cast<LoadInst>(V)){
      Keep = Target = &LI->getOperandUse(0);
   }else if(CastInst* CI = dyn_cast<CastInst>(V)){
      Keep = Target = &CI->getOperandUse(0);
   }else if(GetElementPtrInst* GEP = dyn_cast<GetElementPtrInst>(V)) {
      Keep = Target = &GEP->getOperandUse(0); /* FIXME this is not good because
                                                GEP has many operands*/
   }else{
      Assert(0,*V);
      return NULL;
   }
   while( (Target = Target->getNext()) ){
      //seems all things who use target is after target
      auto U = Target->getUser();
      if(isa<StoreInst>(U) && U->getOperand(1) == Target->get())
         return Target;
      if(CallInst* CI = dyn_cast<CallInst>(U)){
         Argument* arg = findCallInstArgument(CI, Target->get());
         if(arg && !arg->hasNoCaptureAttr() && !arg->onlyReadsMemory()) // adjust attribute
            //if no nocapture and readonly, it means could write into this addr
            return Target;
      }
   }
   //if no use found, we consider this is a constant expr
   if(ConstantExpr* CE = dyn_cast<ConstantExpr>(Keep->get())){
      Instruction* TI = CE->getAsInstruction();
      return (*this)(TI);
   }

   return NULL;
}

/**
 * access whether a Instruction is reference global variable. 
 * if is, return the global variable
 * else return NULL
 */
static Value* access_global_variable(Instruction *I)
{
	Value* Address = NULL, *Test = NULL;
	if(isa<LoadInst>(I) || isa<StoreInst>(I))
		Test = Address = I->getOperand(0);
	else return NULL;
	while(ConstantExpr* CE = dyn_cast<ConstantExpr>(Address)){
		Test = CE->getAsInstruction();
		if(isa<CastInst>(Test))
			Test = Address = castoff(Test);
		else break;
	}
	if(GetElementPtrInst* GEP = dyn_cast<GetElementPtrInst>(Test))
		Test = GEP->getPointerOperand();
	if(isa<GlobalVariable>(Test)) return Address;
	return NULL;
}
/**
 * find where store to a global variable. 
 * require there a only store inst on this global variable.
 * NOTE!! this should replaced with UseOnlyResolve to provide a more correct
 * implement
 */
static void find_global_dependencies(const Value* GV,SmallVectorImpl<FindedDependenciesType>& Result)
{
	for(auto U = GV->use_begin(),E = GV->use_end();U!=E;++U){
		Instruction* I = const_cast<Instruction*>(dyn_cast<Instruction>(*U));
		if(!I){
			find_global_dependencies(*U, Result);
			continue;
		}
		if(isa<StoreInst>(I)){
			Result.push_back(make_pair(MemDepResult::getDef(I),I->getParent()));
		}
	}
}

void MDAResolve::find_dependencies( Instruction* I, const Pass* P,
      SmallVectorImpl<FindedDependenciesType>& Result, NonLocalDepResult* NLDR)
{
   MemDepResult d;
   BasicBlock* SearchPos;
   Instruction* ScanPos;
   MemoryDependenceAnalysis& MDA = P->getAnalysis<MemoryDependenceAnalysis>();
   AliasAnalysis& AA = P->getAnalysis<AliasAnalysis>();

   if(Value* GV = access_global_variable(I)){
      find_global_dependencies(GV, Result);
      return;
   }

   AliasAnalysis::Location Loc;
   if(LoadInst* LI = dyn_cast<LoadInst>(I)){
      Loc = AA.getLocation(LI);
   }else if(StoreInst* SI = dyn_cast<StoreInst>(I))
      Loc = AA.getLocation(SI);
   else
      assert(0);

   if(!NLDR){
      SearchPos = I->getParent();
      d = MDA.getPointerDependencyFrom(Loc, isa<LoadInst>(I), I, SearchPos, I);
      ScanPos = d.getInst();
   }else{
      ScanPos = NLDR->getResult().getInst();
      SearchPos = NLDR->getBB();
      d = NLDR->getResult();
      //we have already visit this BB;
      if(find_if(Result.begin(),Result.end(),[&SearchPos](FindedDependenciesType& f){
               return f.second == SearchPos;}
               ) != Result.end()){
         return;
      }
   }

   while(ScanPos){
      //if isDef, that's what we want
      //if isNonLocal, record BasicBlock to avoid visit twice
      if(d.isDef()||d.isNonLocal())
         Result.push_back(make_pair(d,SearchPos));
      if(d.isDef() && isa<StoreInst>(d.getInst()) )
         return;

      d = MDA.getPointerDependencyFrom(Loc, isa<LoadInst>(I), ScanPos, SearchPos, I);
      ScanPos = d.getInst();
   }

   //if local analysis result is nonLocal 
   //we didn't found a good result, so we continue search
   if(d.isNonLocal()){
      //we didn't record in last time
      Result.push_back(make_pair(d,SearchPos));

      SmallVector<NonLocalDepResult,32> NonLocals;

      MDA.getNonLocalPointerDependency(Loc, isa<LoadInst>(I), SearchPos, NonLocals);
      for(auto r : NonLocals){
         find_dependencies(I, P, Result, &r);
      }
   }else if(d.isNonFuncLocal()){
      //doing some thing here
      Resolver<NoResolve> R;
      bool has_argument = R.resolve_if(I->getOperand(0), [](Value* V){
               return isa<Argument>(V);
            });
      if(has_argument)
         Result.push_back(make_pair(d,SearchPos));
   }
}

Use* MDAResolve::operator()(llvm::Value * V)
{
   Assert(pass,"Not initial with pass");

   SmallVector<FindedDependenciesType, 64> Result;
   Instruction* I = dyn_cast<Instruction>(V);

   if(!I) return NULL;

   find_dependencies(I, pass, Result);

   for(auto R : Result){
      Instruction* Ret = R.first.getInst();
      if(R.first.isDef() && Ret != I)
         return &R.first.getInst()->getOperandUse(0); //FIXME: 应该寻找使用的是哪个参数,而不是直接认为是0
      if(R.first.isNonFuncLocal())
         return NULL;
   }
   return NULL;
}

list<Value*> ResolverBase::direct_resolve( Value* V, unordered_set<Value*>& resolved, function<void(Value*)> lambda)
{
   std::list<llvm::Value*> unresolved;

   if(resolved.find(V) != resolved.end())
      return unresolved;
   if(isa<Argument>(V)){
      unresolved.push_back(V);
   } else if(isa<Constant>(V)){
      resolved.insert(V);
      lambda(V);
   }else if(Instruction* I = dyn_cast<Instruction>(V)){
      if(isa<LoadInst>(I)){
         unresolved.push_back(I);
      }else{
         resolved.insert(I);
         lambda(I);
         uint N = isa<StoreInst>(I)?1:I->getNumOperands();/*if isa store inst,
                                                         we only solve arg 0*/
         for(uint i=0;i<N;i++){
            Value* R = I->getOperand(i);
            auto rhs = direct_resolve(R, resolved, lambda);
            unresolved.insert(unresolved.end(), rhs.begin(), rhs.end());
         }
      }
   }
   return unresolved;
}

void ResolverBase::_empty_handler(llvm::Value *)
{
}

ResolveResult ResolverBase::resolve(llvm::Value* V, std::function<void(Value*)> lambda)
{
   using namespace llvm;
   //bool changed = false;
   std::unordered_set<Value*> resolved;
   std::list<Value*> unsolved;
   std::unordered_map<Value*, Use*> partial;

   unsolved.push_back(NULL); /* always make sure Ite wouldn't point to end();
      因为在一开始的时候,如果没有这句,Ite指向end(),然后又在end后追加数据,造成
      Ite失效.  */

   auto Ite = unsolved.begin();
   Value* next = V; // wait to next resolved;

   while(next || *Ite != NULL){
      if(stop_resolve) break;
      if(next){
         list<Value*> mid = direct_resolve(next, resolved, lambda);
         unsolved.insert(--unsolved.end(),mid.begin(),mid.end()); 
            // insert before NULL, makes NULL always be tail
         next = NULL;
         if(*Ite == NULL) advance(Ite, -mid.size()); /* *Ite == NULL means *Ite points 
            tail, and we insert before it, we should put forware what we inserted */
         continue;
      }

      Use* res = NULL;
      Instruction* I = dyn_cast<Instruction>(*Ite);
      if(!I){
         ++Ite;
         continue;
      }

      res = deep_resolve(I);
      partial.insert(make_pair(I,res));
      if(!res){
         ++Ite;
         continue;
      }
      User* U = res->getUser();

      resolved.insert(I); // original is resolved;
      lambda(I);
      bool dont_erase = false;
      if(isa<StoreInst>(U) || isa<LoadInst>(U)){
         resolved.insert(U);
         lambda(U);
         next = res->get();
      }else if(CallInst* CI = dyn_cast<CallInst>(U)){
         Argument* arg = findCallInstArgument(CI, res->get());
         if(arg && arg->getNumUses()>0){
            resolved.insert(CI);
            lambda(CI);
            next = arg->use_back();
         }else{
            dont_erase = true;
            next = NULL;
         }
      }else
         next = U;

      dont_erase?++Ite:Ite = unsolved.erase(Ite);
   }

   unsolved.pop_back(); // remove last NULL
   return make_tuple(resolved, unsolved, partial);
}

bool ResolverBase::resolve_if(Value *V, function<bool (Value *)> lambda)
{
   stop_resolve = false;
   bool ret = false;

   resolve(V,[&lambda, &ret, this](Value* v){
            if(lambda(v)){
               this->stop_resolve = true;
               ret = true;
            }
         });
   stop_resolve = false;
   return ret;
}


ResolverPass::~ResolverPass(){
   for(auto I : impls)
      delete I.second;
}
