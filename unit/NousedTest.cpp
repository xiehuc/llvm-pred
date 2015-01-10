#include "internal.h"
#include <gtest/gtest.h>

#include "Resolver.h"
#include "util.h"

using namespace llvm;
using namespace lle;

TEST(NousedTest, Linear) {
   const char Assembly[] = R"(
define void @a(){
entry:
   %0 = alloca i32
   store i32 1, i32* %0
   load i32* %0
   ret void
})";
   std::unique_ptr<Module> M = parseAssembly(Assembly);
   Function* F_a = M->getFunction("a");
   BasicBlock::iterator B_a_beg = F_a->begin()->begin();
   StoreInst* SI = cast<StoreInst>(std::next(B_a_beg));

   ResolveEngine RE;
   RE.addRule(RE.ibase_rule);
   RE.addRule(RE.iuse_rule);
   Value* V = RE.find_visit(SI->getOperandUse(1));
   EXPECT_EQ(V, std::next(B_a_beg, 2));
}
