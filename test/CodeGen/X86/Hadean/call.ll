; Test that (a) CALL64m is expanded to MOV+CALL64r, and (b) that CALL64r is instrumented
; with control-flow integrity AND+LEA.

; RUN: llc -print-after-all < %s 2>&1 | FileCheck -check-prefix=CHECK-IR %s
; RUN: llc -filetype obj -verify-machineinstrs < %s | llvm-objdump -d - | FileCheck -check-prefix=CHECK-ASM %s

target triple = "x86_64-hadean-linux"

; Target is the 7th argument, first passed on the stack, to force a load from memory.
define i8* @test(i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, i64 %f, void ()* %target)  {
  tail call void %target()
  unreachable
}

; CHECK-IR:        CALL64m

; CHECK-ASM:       movq    {{[0-9]+}}(%rsp), %[[REG:r..]]
; CHECK-ASM-NEXT:  nopw
; CHECK-ASM-NEXT:  andq    $-32, %[[REG]]
; CHECK-ASM-NEXT:  leaq    (%r15,%[[REG]]), %[[REG]]
; CHECK-ASM-NEXT:  callq   *%[[REG]]
