; ModuleID = '1.add.o'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@a = global i32 0, align 4
@b = global i32 1, align 4

; Function Attrs: nounwind uwtable
define i32 @main(i32 %argc, i8** nocapture %argv) #0 {
  %a.promoted = load i32* @a, align 4, !tbaa !0
  %b.promoted = load i32* @b, align 4, !tbaa !0
  br label %1

; <label>:1                                       ; preds = %1, %0
  %2 = phi i32 [ %b.promoted, %0 ], [ %5, %1 ]
  %3 = phi i32 [ %a.promoted, %0 ], [ %4, %1 ]
  %i.01 = phi i32 [ 0, %0 ], [ %6, %1 ]
  %4 = add nsw i32 %2, %3
  %5 = add nsw i32 %4, %2
  %6 = add nsw i32 %i.01, 1
  %exitcond = icmp eq i32 %6, 10000
  br i1 %exitcond, label %7, label %1

; <label>:7                                       ; preds = %1
  store i32 %4, i32* @a, align 4, !tbaa !0
  store i32 %5, i32* @b, align 4, !tbaa !0
  ret i32 0
}

attributes #0 = { nounwind uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-frame-pointer-elim-non-leaf"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "unsafe-fp-math"="false" "use-soft-float"="false" }

!0 = metadata !{metadata !"int", metadata !1}
!1 = metadata !{metadata !"omnipotent char", metadata !2}
!2 = metadata !{metadata !"Simple C/C++ TBAA"}
