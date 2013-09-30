#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/IRReader/IRReader.h>
#include <stdio.h>

using namespace llvm;
using namespace std;

int main(int argc,char** argv)
{
    if(argc!=2){
        printf("useage %s [source].ll\n",argv[0]);
        return 0;
    }


    LLVMContext cnt;
    SMDiagnostic err;
    Module* mod;
    mod = ParseIRFile(argv[1], err, cnt);

}
