; Test that (a) CALL64m is expanded to MOV+CALL64r, and (b) that CALL64r is instrumented
; with control-flow integrity AND+LEA.

; RUN: llc -hadean-debug-cfi=false -print-after-all < %s 2>&1 | FileCheck -check-prefix=CHECK-IR %s
; RUN: llc -hadean-debug-cfi=true -print-after-all < %s 2>&1 | FileCheck -check-prefix=CHECK-IR %s
; RUN: llc -hadean-debug-cfi=false -filetype obj -verify-machineinstrs < %s | llvm-objdump -d - | FileCheck -check-prefix=CHECK-ASM %s
; RUN: llc -hadean-debug-cfi=true -filetype obj -verify-machineinstrs < %s | llvm-objdump -d - | FileCheck -check-prefix=CHECK-ASM-DBG %s

target triple = "x86_64-hadean-linux"

; Target is the 7th argument, first passed on the stack, to force a load from memory.
define i8* @test(i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, i64 %f, void ()* %target)  {
  tail call void %target()
  unreachable
}

; CHECK-IR:        CALL64m

; CHECK-ASM:       movq    {{[0-9]+}}(%rsp), %[[REG:r..]]
; CHECK-ASM:       andq    $-32, %[[REG]]
; CHECK-ASM-NEXT:  leaq    (%r15,%[[REG]]), %[[REG]]
; CHECK-ASM-NEXT:  callq   *%[[REG]]

; CHECK-ASM-DBG:       movq    {{[0-9]+}}(%rsp), %[[REG:r..]]
; CHECK-ASM-DBG:       xorq    %r15, %[[REG]]
; CHECK-ASM-DBG-NEXT:  testq   $31, %[[REG]]
; CHECK-ASM-DBG-NEXT:  je      2
; CHECK-ASM-DBG-NEXT:  ud2
; CHECK-ASM-DBG-NEXT:  xorq    %r15, %[[REG]]
; CHECK-ASM-DBG-NEXT:  callq   *%[[REG]]
