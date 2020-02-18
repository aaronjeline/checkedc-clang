; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc < %s -mtriple=x86_64-linux -mattr=+clzero | FileCheck %s --check-prefix=X64
; RUN: llc < %s -mtriple=i386-pc-linux -mattr=+clzero | FileCheck %s --check-prefix=X32

define void @foo(i8* %p) #0 {
; X64-LABEL: foo:
; X64:       # %bb.0: # %entry
; X64-NEXT:    movq %rdi, %rax
; X64-NEXT:    clzero
; X64-NEXT:    retq
;
; X32-LABEL: foo:
; X32:       # %bb.0: # %entry
; X32-NEXT:    movl {{[0-9]+}}(%esp), %eax
; X32-NEXT:    clzero
; X32-NEXT:    retl
entry:
  tail call void @llvm.x86.clzero(i8* %p) #1
  ret void
}

declare void @llvm.x86.clzero(i8*) #1
