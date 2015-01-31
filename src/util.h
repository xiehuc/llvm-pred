#ifndef LLE_UTIL_H_H
#define LLE_UTIL_H_H

#include <list>
#include <vector>
#include <functional>
#include <llvm/IR/Value.h>
#include <llvm/IR/Argument.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/ADT/GraphTraits.h>
#include <llvm/Support/raw_ostream.h>

#define WALK_THROUGH_DEPTH 10

namespace lle
{
   // find string expr and priority for CmpInst and BinaryOperator
   const std::pair<const char*, int>& lookup_sym(llvm::CmpInst* CI);
   const std::pair<const char*, int>& lookup_sym(llvm::BinaryOperator* BO);

   // @param expand : true  --> nest expand the load inst's target
   //                 false --> just print the name for load inst's target
	void pretty_print(llvm::Value* v,llvm::raw_ostream& o = llvm::outs(), bool expand = true);

   // check a Argument is write to memory
   // Argument must be PointerType
   // if Function has lle.arg.write attribute. it would consider it first.
   // or it would check argument use to find whether have a store instruction.
   bool isArgumentWrite(llvm::Argument* Arg);

   bool isArray(llvm::Value*);
   // if value is a constexpr refence global variable.
   // return true, and set @param 2 to global variable,
   // if it is referenced by a gep inst, set @param 3 to this gep inst
   bool isRefGlobal(llvm::Value* V, llvm::GlobalVariable** = nullptr, llvm::GetElementPtrInst** = nullptr);
   llvm::GetElementPtrInst* isRefGEP(llvm::Instruction* I);

	//remove cast instruction for a value
	//because cast means the original value and the returned value is
	//semanticly equal
	llvm::Value* castoff(llvm::Value* v);
   //from a given callinst's use, find the function argument
   llvm::Argument* findCallInstArgument(llvm::Use* use);
   // from a given function's argument and callinst, return the use
   inline llvm::Use* findCallInstParameter(llvm::Argument* Arg, llvm::CallInst* CI){
      return &CI->getArgOperandUse(Arg->getArgNo());
   }
   // convert a Use to use_iterator
   inline llvm::Value::use_iterator find_iterator(llvm::Use& U){
      llvm::Value* V = U.get();
      llvm::Value* Ur = U.getUser();
      return std::find_if(V->use_begin(), V->use_end(), [Ur](llvm::Use&U){return U.getUser()==Ur;});
   }

   // convert a use_iterator range to std::vector<Use*>
   template<typename Ite, typename Cont>
   void pushback_to(Ite F, Ite T, Cont& C){
      typedef decltype(*F) UseT;
      std::transform(F, T, std::back_inserter(C), [](UseT& U){return &U;});
   }
   template<typename Ite, typename Cont>
   void insert_to(Ite F, Ite T, Cont& C){
      typedef decltype(*F) UseT;
      std::for_each(F, T, [&C](UseT U){C.insert(U);});
   }

   // insert a constant to module, and return a getelementptr constant expr
   llvm::Constant* insertConstantString(llvm::Module*, const std::string);

   inline uint64_t extract(llvm::ConstantInt* CI){
      return CI?CI->getZExtValue():UINT64_MAX;
   }

   std::vector<llvm::BasicBlock*> getPath(llvm::BasicBlock* From, llvm::BasicBlock* To);
}
namespace std{
// return true if BasicBlock L is 'before' R
template <>
struct less<llvm::BasicBlock>
{
   bool operator()(llvm::BasicBlock*, llvm::BasicBlock* );
};
// return true if Instruction L is 'before' R
template <>
struct less<llvm::Instruction>
{
   bool operator()(llvm::Instruction*, llvm::Instruction* );
};
}

#endif
