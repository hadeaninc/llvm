; Test that (a) TC_RETURN64m is expanded to MOV+TC_RETURN64r, and
; (b) that TC_RETURN64r is instrumented with control-flow integrity AND+LEA.

; RUN: llc -hadean-debug-cfi=false -print-after-all < %s 2>&1 | FileCheck -check-prefix=CHECK-IR %s
; RUN: llc -hadean-debug-cfi=true -print-after-all < %s 2>&1 | FileCheck -check-prefix=CHECK-IR %s
; RUN: llc -hadean-debug-cfi=false -filetype obj -verify-machineinstrs < %s | llvm-objdump -d - | FileCheck -check-prefix=CHECK-ASM %s
; RUN: llc -hadean-debug-cfi=true -filetype obj -verify-machineinstrs < %s | llvm-objdump -d - | FileCheck -check-prefix=CHECK-ASM-DBG %s

target triple = "x86_64-hadean-linux"

; Target is the 7th argument, first passed on the stack, to force load from memory.
define void @test(i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, i64 %f, void ()* %target)  {
  tail call void %target()
  ret void
}

; CHECK-IR:        TCRETURNmi64

; CHECK-ASM:       movq    {{[0-9]+}}(%rsp), %[[REG:r..]]
; CHECK-ASM-NEXT:  andq    $-32, %[[REG]]
; CHECK-ASM-NEXT:  leaq    (%r15,%[[REG]]), %[[REG]]
; CHECK-ASM-NEXT:  jmpq    *%[[REG]]

; CHECK-ASM-DBG:       movq    {{[0-9]+}}(%rsp), %[[REG:r..]]
; CHECK-ASM-DBG-NEXT:  xorq    %r15, %[[REG]]
; CHECK-ASM-DBG-NEXT:  testq   $31, %[[REG]]
; CHECK-ASM-DBG-NEXT:  je      2
; CHECK-ASM-DBG-NEXT:  ud2
; CHECK-ASM-DBG-NEXT:  xorq    %r15, %[[REG]]
; CHECK-ASM-DBG-NEXT:  jmpq    *%[[REG]]
