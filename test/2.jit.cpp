#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>

#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/Support/TargetSelect.h>

#include <sys/time.h>
#include <iostream>

using namespace llvm;
using namespace std;

int main()
{
    InitializeNativeTarget();

    LLVMContext& cnt = getGlobalContext();
    SMDiagnostic diag;
    std::string errstr;
    Module* m = ParseIRFile("../example/3.inst.ll", diag, cnt);
    ExecutionEngine* ee = EngineBuilder(m).setErrorStr(&errstr).create();
    Function * func = m->getFunction("inst_add");
    void* fptr = ee->getPointerToFunction(func);
    void (*fp)() = (void (*)())(intptr_t)fptr;
    struct timeval beg,end;
    gettimeofday(&beg, 0);
    int i;
    for(i=0;i<1000;i++)
        fp();
    gettimeofday(&end, 0);
    cout<<"us:"<<end.tv_usec-beg.tv_usec<<endl;
}
