#ifndef ADAPTIVE_H_H
#define ADAPTIVE_H_H
/** THE ADAPTIVE LAYER FOR LLVM PRED **/
// 由于LLVM基于遍的方式, 最小单位都只能一个函数一个函数的运行. 不利于组合实现.
// 因此只能通过复制官方源代码的方式来访问隐藏的接口, 深度订制.
// 因为需要将LLVM的源文件复制出来, 并利用包含cpp源代码的方式才能使用其隐藏API.
// 为了防止命名污染(包含了复数个cpp), 有必要制作Adaptive层来隔离, 
// 在 ***_Adaptive.cpp 中, 包含对应的cpp, 并且留出必要的接口来实现 **_Adaptive
// 类. 方便其它程序调用.
namespace llvm{
   class AliasAnalysis;
   class AnalysisUsage;
   class DominatorTree;
   class Function;
   class Instruction;
   class FunctionPass;
   class Module;
   class ModulePass;
   class MemoryDependenceAnalysis;
   class Pass;
   class TargetLibraryInfo;
   class BasicBlock;
}

namespace lle{
//the adaptive for llvm's DeadStoreElimination
struct DSE_Adaptive{
   void* opaque;
   llvm::MemoryDependenceAnalysis* MD;
   llvm::AliasAnalysis* AA;
   llvm::DominatorTree* DT;
   const llvm::TargetLibraryInfo* TLI;

   DSE_Adaptive(llvm::FunctionPass* DSE);
   void prepare(llvm::Pass* M);
   void prepare(llvm::Function* F, llvm::Pass* P);
   void getAnalysisUsage(llvm::AnalysisUsage& AU) const;
   void runOnBasicBlock(llvm::BasicBlock& BB);
   void DeleteDeadInstruction(llvm::Instruction* I);
};

struct DAE_Adaptive{
   void* opaque;

   DAE_Adaptive(llvm::ModulePass* DAE);
   void prepare(llvm::Module* M);
   void runOnFunction(llvm::Function& F);
};

struct InstCombine_Adaptive{
   void* opaque;

   InstCombine_Adaptive(llvm::FunctionPass* IC);
   const void* id() const;
   void getAnalysisUsage(llvm::AnalysisUsage& AU) const;
   void prepare(llvm::Pass* P);
   void runOnFunction(llvm::Function& F);

};
}

#endif
