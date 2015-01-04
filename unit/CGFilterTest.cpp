#include <llvm/IR/Module.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/AsmParser/Parser.h>
#include <gtest/gtest.h>
#include <memory>

#include "Resolver.h"
#include "util.h"
using namespace llvm;
using namespace lle;
namespace {

std::unique_ptr<Module> parseAssembly(const char *Assembly) {
  auto M = make_unique<Module>("Module", getGlobalContext());

  SMDiagnostic Error;
  bool Parsed =
      ParseAssemblyString(Assembly, M.get(), Error, M->getContext()) == M.get();

  std::string ErrMsg;
  raw_string_ostream OS(ErrMsg);
  Error.print("", OS);

  // A failure here means that the test itself is buggy.
  if (!Parsed)
    report_fatal_error(OS.str().c_str());

  return M;
}
}

TEST(CGFilterTest, OrderTest) {
   const char Assembly[] = R"(
define void @a(){
entry:
   %0 = bitcast i32 0 to i32
   %1 = trunc   i32 1 to i1
   %2 = bitcast i32 2 to i32
   br i1 %1, label %"b1", label %"b2"
b1:
   %3 = bitcast i32 3 to i32
   ret void
b2:
   %4 = bitcast i32 4 to i32
   ret void
})";
   std::unique_ptr<Module> M = parseAssembly(Assembly);
   Function* F_a = M->getFunction("a");
   BasicBlock::iterator B_a_beg = F_a->begin()->begin();
   BasicBlock::iterator B_b1_beg = std::next(F_a->begin())->begin();
   BasicBlock::iterator B_b2_beg = std::next(F_a->begin(), 2)->begin();

   EXPECT_TRUE(std::less<Instruction>()(B_a_beg, std::next(B_a_beg)));
   EXPECT_TRUE(std::less<Instruction>()(B_a_beg, std::next(B_a_beg, 2)));
   EXPECT_TRUE(std::less<Instruction>()(std::next(B_a_beg), B_b1_beg));
   EXPECT_TRUE(std::less<Instruction>()(B_b1_beg, B_b2_beg));
}

TEST(CGFilterTest, Construct) {
   const char Assembly[] = R"(
define void @a(){
entry:
   %0 = bitcast i32 0 to i32
   call void @b()
   %1 = bitcast i32 1 to i32
   call void @c()
   %2 = bitcast i32 2 to i32
   ret void
}
define void @b(){
entry:
   ret void
}
define void @c(){
entry:
   ret void
})";
   std::unique_ptr<Module> M = parseAssembly(Assembly);
   Function* F_a = M->getFunction("a");
   CallGraph CG(*M);
   CallGraphNode* CG_root = CG[F_a];
   BasicBlock::iterator B_a_beg = F_a->begin()->begin();
   Instruction* First_inst = dyn_cast<Instruction>(B_a_beg);
   EXPECT_NE(First_inst, nullptr);
   CGFilter CGF(CG_root, First_inst);
   EXPECT_EQ(CGF.order_map.size(), 3);
   /**callgraph: a   --- 0
    *            |-b --- 1
    *            |-c --- 2
    */
   EXPECT_EQ(CGF.order_map[M->getFunction("a")], 0);
   EXPECT_EQ(CGF.order_map[M->getFunction("b")], 1);
   EXPECT_EQ(CGF.order_map[M->getFunction("c")], 2);

   // %0 = bitcast i32 0 to i32
   EXPECT_EQ(CGF.threshold, 0);

   // call void @b()
   EXPECT_NE(dyn_cast<Instruction>(std::next(B_a_beg)), nullptr);
   CGF.update(std::next(B_a_beg));
   EXPECT_EQ(CGF.threshold, 1);

   // %1 = bitcast i32 1 to i32
   CGF.update(std::next(B_a_beg, 2));
   EXPECT_EQ(CGF.threshold, 1);

   CGF.update(std::next(B_a_beg, 3));
   EXPECT_EQ(CGF.threshold, 2);
}

