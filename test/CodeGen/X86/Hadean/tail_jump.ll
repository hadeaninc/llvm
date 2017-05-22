; Test that (a) TC_RETURN64m is expanded to MOV+TC_RETURN64r, and
; (b) that TC_RETURN64r is instrumented with control-flow integrity AND+LEA.

; RUN: llc -print-after-all < %s 2>&1 | FileCheck -check-prefix=CHECK-IR %s
; RUN: llc -filetype obj -verify-machineinstrs < %s | llvm-objdump -d - | FileCheck -check-prefix=CHECK-ASM %s

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
