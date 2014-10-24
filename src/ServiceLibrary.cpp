#include "preheader.h"
#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>

#include "debug.h"


namespace lle{
   /* Make a Module as Service Library
    * which all function is always-inline
    * convient for other module call it's function
    */
   class ServiceLibrary: public llvm::ModulePass
   {
      public:
      static char ID;
      ServiceLibrary():ModulePass(ID) {}
      bool runOnModule(llvm::Module& M) override {
         for (llvm::Function& F : M ){
            if(!F.isDeclaration())
               F.addFnAttr(llvm::Attribute::AlwaysInline);
         }
         return false;
      }
   };
}

char lle::ServiceLibrary::ID = 0;
llvm::RegisterPass<lle::ServiceLibrary> X("ServLib","Make a Module as a Service, which all function is always-inline");
