#include "internal.h"
#include <gtest/gtest.h>

#include "Resolver.h"
#include "ddg.h"
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
   RE.addRule(InitRule(RE.iuse_rule));

   Value* V;
   RE.resolve(&SI->getOperandUse(1), RE.findVisit(V));
   EXPECT_EQ(V, std::next(B_a_beg, 2));
}

TEST(NousedTest, GepBeforeSucc) {
   const char Assembly[] = R"(
%struct.ST = type { i32, double, [4 x i32] }
declare void @test(%struct.ST*)
declare void @test2(i32*)
define void @a(){
entry:
   %val = alloca %struct.ST, align 8
   %0 = getelementptr inbounds %struct.ST* %val, i32 0, i32 0
   %1 = load i32* %0
   call void @test(%struct.ST* %val)
   %2 = load i32* %0
   %3 = bitcast %struct.ST* %val to i8*
   %4 = load i32* %0
   ret void
exp2:
   %v2 = alloca %struct.ST, align 8
   %5 = getelementptr inbounds %struct.ST* %v2, i32 0, i32 0
   call void @test2(i32* %6)
   %6 = getelementptr inbounds %struct.ST* %v2, i32 0, i32 0
   %7 = load i32* %6
   ret void
})";
   std::unique_ptr<Module> M = parseAssembly(Assembly);
   Function::iterator F_a = M->getFunction("a")->begin();
   BasicBlock::iterator B_a_beg = F_a->begin();
   BasicBlock::iterator B_b_beg = ++F_a->begin();
   CallInst* Test = cast<CallInst>(std::next(B_a_beg, 3));
   CallInst* Test2 = cast<CallInst>(std::next(B_b_beg, 2));

   ResolveEngine RE;
   RE.addRule(RE.ibase_rule);
   InitRule ir(RE.iuse_rule);
   RE.addRule(std::ref(ir));

   Value* V = RE.find_visit(&Test->getArgOperandUse(0));
   EXPECT_EQ(V, std::next(B_a_beg, 6)); // %4

   /*
   ir.clear();
   V = RE.find_visit(&Test2->getArgOperandUse(0));
   EXPECT_EQ(V, std::next(B_b_beg, 4)); 
   errs()<<*V<<"\n";
   */
}

TEST(NousedTest, GepBeforeFail) {
   const char Assembly[] = R"(
%struct.ST = type { i32, double, [4 x i32] }
declare void @test(%struct.ST*)
declare void @test2(i32*)
define void @a(){
entry:
   %val = alloca %struct.ST, align 8
   %0 = getelementptr inbounds %struct.ST* %val, i32 0, i32 0
   %1 = load i32* %0
   call void @test(%struct.ST* %val)
   ret void
exp2:
   %v2 = alloca %struct.ST, align 8
   %2 = getelementptr inbounds %struct.ST* %v2, i32 0, i32 0
   %3 = load i32* %2
   call void @test2(i32* %2)
   ret void
exp3:
   %v3 = alloca %struct.ST, align 8
   %4 = getelementptr inbounds %struct.ST* %v3, i32 0, i32 0
   call void @test2(i32* %4)
   ret void
})";
   std::unique_ptr<Module> M = parseAssembly(Assembly);
   Function::iterator F_a = M->getFunction("a")->begin();
   BasicBlock::iterator B_a_beg = F_a->begin();
   LoadInst* LI = cast<LoadInst>(std::next(B_a_beg, 2));
   CallInst* Test = cast<CallInst>(std::next(B_a_beg, 3));
   CallInst* Test2 = cast<CallInst>(std::next((++F_a)->begin(), 3));
   CallInst* Test3 = cast<CallInst>(std::next((++F_a)->begin(), 2));

   ResolveEngine RE;
   RE.addRule(RE.ibase_rule);
   RE.addRule(InitRule(RE.iuse_rule));
   EXPECT_TRUE(std::less<Instruction>()(LI,Test));

   Use* Q = &Test->getArgOperandUse(0);
   iUseFilter uf(Q);
   RE.addFilter(std::ref(uf));

   Value* V;
   RE.resolve(Q, RE.findVisit(V));
   EXPECT_EQ(V, nullptr);

   auto GEP = isGEP(Test2->getArgOperandUse(0));
   RE.addFilter(GEPFilter(GEP));
   Q = &Test2->getArgOperandUse(0);
   uf.update(Q);
   RE.resolve(Q, RE.findVisit(V));
   EXPECT_EQ(V, nullptr);

   Q = &Test3->getArgOperandUse(0);
   uf.update(Q);
   RE.resolve(Q, RE.findVisit(V));
   EXPECT_EQ(V, nullptr);
}
