; ModuleID = '1.fibonacci.o'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [5 x i8] c"%lu\0A\00", align 1

; Function Attrs: nounwind uwtable
define i32 @main(i32 %argc, i8** %argv) #0 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i8**, align 8
  %num = alloca i64, align 8
  %a = alloca i64, align 8
  %b = alloca i64, align 8
  %i = alloca i64, align 8
  %c = alloca i64, align 8
  store i32 0, i32* %1
  store i32 %argc, i32* %2, align 4
  store i8** %argv, i8*** %3, align 8
  %4 = load i32* %2, align 4
  %5 = icmp ne i32 %4, 2
  br i1 %5, label %6, label %7

; <label>:6                                       ; preds = %0
  store i32 -1, i32* %1
  br label %28

; <label>:7                                       ; preds = %0
  %8 = load i8*** %3, align 8
  %9 = getelementptr inbounds i8** %8, i64 1
  %10 = load i8** %9, align 8
  %11 = call i64 @atol(i8* %10) #3
  store i64 %11, i64* %num, align 8
  store i64 1, i64* %a, align 8
  store i64 2, i64* %b, align 8
  store i64 0, i64* %i, align 8
  store i64 0, i64* %c, align 8
  store i64 0, i64* %i, align 8
  br label %12

; <label>:12                                      ; preds = %22, %7
  %13 = load i64* %i, align 8
  %14 = load i64* %num, align 8
  %15 = icmp ult i64 %13, %14
  br i1 %15, label %16, label %25

; <label>:16                                      ; preds = %12
  %17 = load i64* %a, align 8
  %18 = load i64* %b, align 8
  %19 = add i64 %17, %18
  store i64 %19, i64* %c, align 8
  %20 = load i64* %b, align 8
  store i64 %20, i64* %a, align 8
  %21 = load i64* %c, align 8
  store i64 %21, i64* %b, align 8
  br label %22

; <label>:22                                      ; preds = %16
  %23 = load i64* %i, align 8
  %24 = add i64 %23, 1
  store i64 %24, i64* %i, align 8
  br label %12

; <label>:25                                      ; preds = %12
  %26 = load i64* %c, align 8
  %27 = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([5 x i8]* @.str, i32 0, i32 0), i64 %26)
  br label %28

; <label>:28                                      ; preds = %25, %6
  %29 = load i32* %1
  ret i32 %29
}

; Function Attrs: nounwind readonly
declare i64 @atol(i8*) #1

declare i32 @printf(i8*, ...) #2

attributes #0 = { nounwind uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf"="true" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readonly "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf"="true" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf"="true" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nounwind readonly }
