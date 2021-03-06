; REQUIRES: x86
; RUN: llvm-as %S/Inputs/archive.ll -o %t1.o
; RUN: rm -f %t.a
; RUN: llvm-ar rcs %t.a %t1.o
; RUN: llvm-as %s -o %t2.o
; RUN: ld.lld %t2.o %t.a -o %t3 -shared
; RUN: llvm-readobj --symbols %t3 | FileCheck %s
; RUN: ld.lld %t2.o --whole-archive %t.a -o %t3 -shared
; RUN: llvm-readobj --symbols %t3 | FileCheck %s

; CHECK:      Name: g (
; CHECK-NEXT: Value:
; CHECK-NEXT: Size:
; CHECK-NEXT: Binding: Global
; CHECK-NEXT: Type: Function
; CHECK-NEXT: Other: 0
; CHECK-NEXT: Section: .text

; CHECK:      Name: f (
; CHECK-NEXT: Value:
; CHECK-NEXT: Size:
; CHECK-NEXT: Binding: Global
; CHECK-NEXT: Type: Function
; CHECK-NEXT: Other: 0
; CHECK-NEXT: Section: .text

target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define void @g() {
  call void @f()
  ret void
}

declare void @f()

