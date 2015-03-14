#include "internal.h"
#include <gtest/gtest.h>

#include "util.h"

using namespace lle;
using namespace llvm;
using namespace testing;

static const char Assembly[] = R"(
%struct.ST = type { i32, double, [4 x i32] }

define i32 @main() #0 {
"1":
  br i1 0, label %"2", label %"3"

"2":
  br i1 1, label %"4", label %"5"

"3":
  br label %"6"

"4":
  ret i32 0

"5":
  ret i32 0

"6":
  ret i32 0
})";

static std::unique_ptr<Module> M = parseAssembly(Assembly);

TEST(UtilTest, getPath) {
   auto B_beg = M->getFunction("main")->begin();
   auto Path = getPath(B_beg, std::next(B_beg, 3));

   ASSERT_EQ(Path.size(), 3);
   EXPECT_EQ(Path[0], B_beg);
   EXPECT_EQ(Path[1], std::next(B_beg));
   EXPECT_EQ(Path[2], std::next(B_beg, 3));
}

// GCD function in PerformPred.cpp
static uint32_t GCD(uint32_t A, uint32_t B) // 最大公约数
{
#define swap(A,B) (T=A,A=B,B=T)
   uint32_t T;
   A>B?swap(A,B):0;//makes A is small, B is large
   if(A==0) return 0;
   do{
      B%=A;
      A>B?swap(A,B):0;
   }while(A!=0);
   return B;
#undef swap
}

TEST(UtilTest, GCDTest) {
   EXPECT_EQ(GCD(2,6),  2);
   EXPECT_EQ(GCD(8,12), 4);
   // Zero Test
   EXPECT_EQ(GCD(0,12), 0);
   EXPECT_EQ(GCD(12,0), 0);
}
