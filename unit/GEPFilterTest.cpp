#include "internal.h"
#include <gtest/gtest.h>

#include "Resolver.h"

using namespace lle;
using namespace llvm;
using namespace testing;

static const char Assembly[] = R"(
%struct.ST = type { i32, double, [4 x i32] }

define i32 @main() #0 {
  %1 = alloca i32, align 4
  %val = alloca %struct.ST, align 8
  %i = alloca i32, align 4
  store i32 0, i32* %1
  %2 = getelementptr inbounds %struct.ST* %val, i32 0, i32 0
  store i32 0, i32* %2, align 4
  %3 = getelementptr inbounds %struct.ST* %val, i32 0, i32 1
  store double 1.000000e+00, double* %3, align 8
  store i32 0, i32* %i, align 4
  br label %4

; <label>:4                                       ; preds = %13, %0
  %5 = load i32* %i, align 4
  %6 = icmp slt i32 %5, 4
  br i1 %6, label %7, label %15

; <label>:7                                       ; preds = %4
  %8 = load i32* %i, align 4
  %9 = load i32* %i, align 4
  %10 = sext i32 %9 to i64
  %11 = getelementptr %struct.ST* %val, i32 0, i32 2, i64 %10
  store i32 %8, i32* %11, align 4
  br label %12

; <label>:13                                      ; preds = %7
  %13 = load i32* %i, align 4
  %14 = add nsw i32 %13, 1
  store i32 %14, i32* %i, align 4
  br label %4

; <label>:16                                      ; preds = %4
  ret i32 0
})";

static std::unique_ptr<Module> M = parseAssembly(Assembly);

TEST(GEPFilterTest, Construct) {
   GEPFilter gf_a{0,1,2};
   uint64_t arr[] = {0,1,2};
   GEPFilter gf_b(arr);
   auto B_beg = M->getFunction("main")->begin();
   auto inst_0 = dyn_cast<GetElementPtrInst>(std::next(B_beg->begin(), 6));
   GEPFilter gf_c(inst_0);
   std::advance(B_beg, 2);
   auto inst_1 = dyn_cast<GetElementPtrInst>(std::next(B_beg->begin(), 3));
   // not all constant get element ptr
   GEPFilter gf_d(inst_1);

   for(unsigned i=0;i<3;i++){
      EXPECT_EQ(gf_a.idxs[i], arr[i]);
      EXPECT_EQ(gf_b.idxs[i], arr[i]);
   }
   EXPECT_EQ(gf_c.idxs[1], 1);
   EXPECT_EQ(gf_d.idxs[1], 2);
}

TEST(GEPFilterTest, Query) {
   GEPFilter gf_a{0,1};
   GEPFilter gf_b{0,2};

   auto B_beg = M->getFunction("main")->begin();
   Instruction* inst_0 = std::next(B_beg->begin(), 4);
   Instruction* inst_1 = std::next(B_beg->begin(), 6);
   std::advance(B_beg, 2);
   Instruction* inst_2 = std::next(B_beg->begin(), 3);

   EXPECT_TRUE (gf_a(&inst_0->getOperandUse(0)));
   EXPECT_FALSE(gf_a(&inst_1->getOperandUse(0)));
   EXPECT_TRUE (gf_a(&inst_2->getOperandUse(0)));

   EXPECT_TRUE (gf_b(&inst_0->getOperandUse(0)));
   EXPECT_TRUE (gf_b(&inst_1->getOperandUse(0)));
   EXPECT_FALSE(gf_b(&inst_2->getOperandUse(0)));


}
