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

inline unsigned order_map(CGFilter& CGF, Function* F) {
   return CGF.order_map[F].first;
}
}

// test for std::less<Instruction>
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

TEST(CGFilterTest, NestConstruct) {
   const char Assembly[] = R"(
define void @a(){  ;/** @a : 0 
entry:             ; *       call @b: 1
   call void @b()  ; *       |        call @e: 2          
   call void @c()  ; *       |        3                    
   call void @d()  ; *       |        call @f: 4           
   ret void        ; *       ---------5
}                  ; *       6
define void @b(){  ; *       call @c: 7
entry:             ; *       8
   call void @e()  ; *       call @d: 9                    
   call void @f()  ; *       10                            
   ret void        ; */                          
}
define void @c(){
entry:
   ret void
}
define void @d(){
entry:
   ret void
}
define void @e(){
entry:
   ret void
}
define void @f(){
entry:
   ret void
})";
   std::unique_ptr<Module> M = parseAssembly(Assembly);
   Function* F_a = M->getFunction("a");
   CallGraph CG(*M);
   CallGraphNode* CG_root = CG[F_a];
   CGFilter CGF(CG_root, nullptr);
   auto B_a = F_a->begin()->begin();
   Instruction* Call_b = B_a++;
   Instruction* Call_c = B_a++;
   Instruction* Call_d = B_a++;
   auto B_b = M->getFunction("b")->begin()->begin();
   Instruction* Call_e = B_b++;
   Instruction* Call_f = B_b++;

   // 我们需要为空白的地方预留空间.
   EXPECT_EQ(order_map(CGF, M->getFunction("a")), 0);
   EXPECT_EQ(order_map(CGF, M->getFunction("b")), 1);
   EXPECT_EQ(order_map(CGF, M->getFunction("e")), 2);
   EXPECT_EQ(order_map(CGF, M->getFunction("f")), 4);
   EXPECT_EQ(order_map(CGF, M->getFunction("c")), 7);
   EXPECT_EQ(order_map(CGF, M->getFunction("d")), 9);

   EXPECT_EQ(CGF.indexof(Call_b), 0);
   EXPECT_EQ(CGF.indexof(Call_c), 6);
   EXPECT_EQ(CGF.indexof(Call_d), 8);
   EXPECT_EQ(CGF.indexof(Call_e), 1);
   EXPECT_EQ(CGF.indexof(Call_f), 3);
}

TEST(CGFilterTest, TriangleConstruct) {
   const char Assembly[] = R"(
define void @a(){  ;/** @a : 0 
entry:             ; *       call @b: 1
   %0 = alloca i32 ; *       |        call @e: 2          
   call void @b()  ; *       |        3   
   %1 = alloca i32 ; *       |        call @c: 4 
   call void @c()  ; *       ---------5                   
   %2 = alloca i32 ; *       6
   call void @d()  ; *       call @c: 4          
   %3 = alloca i32 ; *       6
   ret void        ; *       call @d: 7
}                  ; *       8 
define void @b(){  
entry:             
   %0 = alloca i32
   call void @e()     
   %1 = alloca i32
   call void @c()                             
   %2 = alloca i32
   ret void                         
}
define void @c(){
entry:
   %0 = alloca i32
   ret void
}
define void @d(){
entry:
   %0 = alloca i32
   ret void
}
define void @e(){
entry:
   %0 = alloca i32
   ret void
})";
   std::unique_ptr<Module> M = parseAssembly(Assembly);
   Function* F_a = M->getFunction("a");
   CallGraph CG(*M);
   CallGraphNode* CG_root = CG[F_a];
   CGFilter CGF(CG_root, nullptr);
   auto B_a = F_a->begin()->begin();
   auto B_b = M->getFunction("b")->begin()->begin();
   auto B_c = M->getFunction("c")->begin()->begin();
   auto B_d = M->getFunction("d")->begin()->begin();
   auto B_e = M->getFunction("e")->begin()->begin();

   EXPECT_EQ(order_map(CGF, M->getFunction("a")), 0);
   EXPECT_EQ(order_map(CGF, M->getFunction("b")), 1);
   EXPECT_EQ(order_map(CGF, M->getFunction("e")), 2);
   EXPECT_EQ(order_map(CGF, M->getFunction("c")), 4);
   EXPECT_EQ(order_map(CGF, M->getFunction("d")), 7);

   EXPECT_EQ(CGF.indexof(B_a), 0);
   std::advance(B_a, 2);
   EXPECT_EQ(CGF.indexof(B_a), 6);
   std::advance(B_a, 2);
   EXPECT_EQ(CGF.indexof(B_a), 6);
   std::advance(B_a, 2);
   EXPECT_EQ(CGF.indexof(B_a), 8);
   EXPECT_EQ(CGF.indexof(B_b), 1);
   std::advance(B_b, 2);
   EXPECT_EQ(CGF.indexof(B_b), 3);
   std::advance(B_b, 2);
   EXPECT_EQ(CGF.indexof(B_b), 5);
   EXPECT_EQ(CGF.indexof(B_e), 2);
   EXPECT_EQ(CGF.indexof(B_c), 4);
   EXPECT_EQ(CGF.indexof(B_d), 7);
}

TEST(CGFilterTest, RecusiveConstruct) {
   const char Assembly[] = R"(
define void @a(){  ;/** @a : 0 
entry:             ; *       call @b: 1
   call void @b()  ; *       2          
   ret void        ; *       
}                  ; *       
define void @b(){  ; *       
entry:             ; *       
   call void @a()  ; *       
   ret void        ; */                         
})";
   std::unique_ptr<Module> M = parseAssembly(Assembly);
   Function* F_a = M->getFunction("a");
   CallGraph CG(*M);
   CallGraphNode* CG_root = CG[F_a];
   CGFilter CGF(CG_root, nullptr);

   EXPECT_EQ(order_map(CGF, M->getFunction("a")), 0);
   EXPECT_EQ(order_map(CGF, M->getFunction("b")), 1);
}

TEST(CGFilterTest, ConstructFromRoot) {
   const char Assembly[] = R"(
define void @a(){
entry:
   %0 = bitcast i32 0 to i32 ; -- 0 
   call void @b()            ; -- 0
   %1 = bitcast i32 1 to i32 ; -- 2
   call void @c()            ; -- 2
   %2 = bitcast i32 2 to i32 ; -- 4
   ret void
}
define void @b(){
entry:
   ret void   ; -- 1
}
define void @c(){
entry:
   ret void   ; -- 3
})";
   std::unique_ptr<Module> M = parseAssembly(Assembly);
   Function* F_a = M->getFunction("a");
   CallGraph CG(*M);
   CallGraphNode* CG_root = CG[F_a];
   BasicBlock::iterator B_a_beg = F_a->begin()->begin();
   Instruction* Bitcast_0 = dyn_cast<Instruction>(B_a_beg++);
   EXPECT_NE(Bitcast_0, nullptr);
   Instruction* Bitcast_1 = dyn_cast<Instruction>(B_a_beg++);
   Instruction* Bitcast_2 = dyn_cast<Instruction>(B_a_beg++);
   CGFilter CGF(CG_root, Bitcast_0);
   EXPECT_EQ(CGF.order_map.size(), 3);

   EXPECT_EQ(order_map(CGF, M->getFunction("a")), 0);
   EXPECT_EQ(order_map(CGF, M->getFunction("b")), 1);
   EXPECT_EQ(order_map(CGF, M->getFunction("c")), 3);

   EXPECT_EQ(CGF.indexof(Bitcast_0), 0);
   EXPECT_EQ(CGF.indexof(Bitcast_1), 2);
   EXPECT_EQ(CGF.indexof(Bitcast_2), 4);

   // %1 = bitcast i32 1 to i32
   CGF.update(Bitcast_1);
   EXPECT_EQ(CGF.threshold, 2);
   EXPECT_TRUE(CGF(&Bitcast_0->getOperandUse(0)));
   EXPECT_TRUE(CGF(&Bitcast_1->getOperandUse(0)));
   EXPECT_FALSE(CGF(&Bitcast_2->getOperandUse(0)));
}

TEST(CGFilterTest, TriangleQuery) {
   const char Assembly[] = R"(
define void @a(){ ;--0
entry:
   %0 = bitcast i32 0 to i32  ; -- 0
   call void @b()             ; -- 0
   %1 = bitcast i32 1 to i32  ; -- 4
   call void @c()             ; -- 4
   %2 = bitcast i32 2 to i32  ; -- 4
   ret void
}                             ; -- 5
define void @b(){
entry:
   %0 = bitcast i32 3 to i32  ; -- 1
   call void @c()             ; -- 1
   %1 = bitcast i32 4 to i32  ; -- 3
   ret void
}                             ; -- 4
define void @c(){
entry:
   %0 = bitcast i32 5 to i32  ; -- 2
   ret void                   
})";
   std::unique_ptr<Module> M = parseAssembly(Assembly);
   Function* F_a = M->getFunction("a");
   Function* F_b = M->getFunction("b");
   Function* F_c = M->getFunction("c");
   BasicBlock::iterator B_a_beg = F_a->begin()->begin();
   BasicBlock::iterator B_b_beg = F_b->begin()->begin();
   BasicBlock::iterator B_c_beg = F_c->begin()->begin();
   Instruction* Bitcast_0 = B_a_beg++;
   Instruction* Bitcast_1 = ++B_a_beg;
   Instruction* Bitcast_2 = std::next(B_a_beg, 2);
   Instruction* Bitcast_3 = B_b_beg;
   Instruction* Bitcast_4 = std::next(B_b_beg, 2);
   Instruction* Bitcast_5 = B_c_beg;

   CallGraph CG(*M);
   CGFilter CGF(CG[F_a], Bitcast_3);
   EXPECT_EQ(CGF.order_map[F_a].last, 5);
   EXPECT_EQ(CGF.order_map[F_b].last, 4);
   EXPECT_EQ(CGF.order_map[F_c].last, 3);

   EXPECT_EQ(CGF.indexof(Bitcast_0), 0);
   EXPECT_EQ(CGF.indexof(Bitcast_1), 4);
   EXPECT_EQ(CGF.indexof(Bitcast_2), 4);
   EXPECT_EQ(CGF.indexof(Bitcast_3), 1);
   EXPECT_EQ(CGF.indexof(Bitcast_4), 3);
   EXPECT_EQ(CGF.indexof(Bitcast_5), 2);

   EXPECT_TRUE (CGF(&Bitcast_0->getOperandUse(0)));
   EXPECT_FALSE(CGF(&Bitcast_1->getOperandUse(0)));
   EXPECT_FALSE(CGF(&Bitcast_2->getOperandUse(0)));
   EXPECT_TRUE (CGF(&Bitcast_3->getOperandUse(0)));
   EXPECT_FALSE(CGF(&Bitcast_4->getOperandUse(0)));
   EXPECT_FALSE(CGF(&Bitcast_5->getOperandUse(0)));

   CGF.update(Bitcast_4);
   EXPECT_TRUE (CGF(&Bitcast_0->getOperandUse(0)));
   EXPECT_FALSE(CGF(&Bitcast_1->getOperandUse(0)));
   EXPECT_FALSE(CGF(&Bitcast_2->getOperandUse(0)));
   EXPECT_TRUE (CGF(&Bitcast_3->getOperandUse(0)));
   EXPECT_TRUE (CGF(&Bitcast_4->getOperandUse(0)));
   EXPECT_TRUE (CGF(&Bitcast_5->getOperandUse(0)));
}
