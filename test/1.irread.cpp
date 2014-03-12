#include "config.h"
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IRReader/IRReader.h>

#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/InstIterator.h>

#include <iostream>
#include <map>

using namespace llvm;
using namespace std;

static map<unsigned,int> inst_counter;

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

    for (const auto& func : *mod){
        for(const_inst_iterator inst=inst_begin(func),
                inst_e=inst_end(func);inst!=inst_e;inst++){
            inst_counter[inst->getOpcode()]++;
        }
    }

    for(const auto& pair : inst_counter){
        cout<<Instruction::getOpcodeName(pair.first)<<":"<<pair.second<<endl;
    }

}
