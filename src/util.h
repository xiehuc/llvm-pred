#ifndef LLE_UTIL_H_H
#define LLE_UTIL_H_H

#include <list>
#include <vector>
#include <llvm/IR/Value.h>
#include <llvm/IR/Argument.h>
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

	//remove cast instruction for a value
	//because cast means the original value and the returned value is
	//semanticly equal
	llvm::Value* castoff(llvm::Value* v);
   //from a given callinst's use, find the function argument
   llvm::Argument* findCallInstArgument(llvm::Use* use);
   // from a given function's argument and callinst, return the use
   inline llvm::Use* findCallInstParameter(llvm::Argument* Arg, llvm::CallInst* CI){
      return &CI->getOperandUse(Arg->getArgNo());
   }
   //find where to use V on instuction I
   inline llvm::Use* findOpUseOnInstruction(llvm::Instruction* I, llvm::Value* V){
      for(auto O = I->op_begin(), E = I->op_end(); O!=E; ++O){
         if(O->get() == V) return &*O;
      }
      return nullptr;
   }

   // inspired from llvm::ErrorOr<>
   template<typename FirstT, typename SecondT>
   class union_pair
   {
      union {
         llvm::AlignedCharArrayUnion<SecondT> SStorage;
         llvm::AlignedCharArrayUnion<FirstT> FStorage;
      };
      bool isFirst:1;
      FirstT* getFirstStorage(){
         return reinterpret_cast<FirstT*>(FStorage.buffer);
      }
      SecondT* getSecondStorage(){
         return reinterpret_cast<SecondT*>(SStorage.buffer);
      }
      public:
      union_pair(const FirstT& first):isFirst(true) {
         //placement new
         new (getFirstStorage()) FirstT(first);
      }
      union_pair(const SecondT& second):isFirst(false) {
         new (getSecondStorage()) SecondT(second);
      }
      union_pair():union_pair(FirstT()) {}
      union_pair(const union_pair& Other) {
         if(Other.isFirst){
            isFirst = true;
            new (getFirstStorage()) FirstT(*Other.getFirstStorage());
         }else{
            isFirst = false;
            new (getSecondStorage()) SecondT(*Other.getSecondStorage());
         }
      }
      union_pair(union_pair&& Other) {
         if(Other.isFirst){
            isFirst = true;
            new (getFirstStorage())
               FirstT(std::move(*Other.getFirstStorage()));
         }else{
            isFirst = false;
            new (getSecondStorage())
               SecondT(std::move(*Other.getSecondStorage()));
         }
      }
      ~union_pair(){
         if(isFirst)
            getFirstStorage()->~FirstT();
         else
            getSecondStorage()->~SecondT();
      }

      union_pair& operator=(const union_pair& Other){
         this->~union_pair();
         new (this) union_pair(Other);
         return *this;
      }
      union_pair& operator=(union_pair&& Other){
         this->~union_pair();
         new (this) union_pair(std::move(Other));
         return *this;
      }
   };
}

#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR == 4
#define user_back(ins)  ins->use_back()
#define user_begin(ins) ins->use_begin()
#define user_end(ins)   ins->use_end()
#else
#define user_back(ins)  ins->user_back()
#define user_begin(ins) ins->user_begin()
#define user_end(ins)   ins->user_end()
#endif

#endif
