; Test that (a) JMP64m is expanded to MOV+JMP64r, and (b) that JMP64r is instrumented
; with control-flow integrity AND+LEA.

; RUN: llc -print-after-all < %s 2>&1 | FileCheck -check-prefix=CHECK-IR %s
; RUN: llc -filetype obj -verify-machineinstrs < %s | llvm-objdump -d - | FileCheck -check-prefix=CHECK-ASM %s

target triple = "x86_64-hadean-linux"

declare void @llvm.trap() noreturn nounwind

; Target is the 7th argument, first passed on the stack, to force a load from memory.
define i8* @test(i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, i64 %f, i8* %target)  {
  indirectbr i8* %target, [ label %label1 ]

label1:
  call void @llvm.trap()
  unreachable
}

; CHECK-IR:        JMP64m

; CHECK-ASM:       movq    {{[0-9]+}}(%rsp), %[[REG:r..]]
; CHECK-ASM-NEXT:  andq    $-32, %[[REG]]
; CHECK-ASM-NEXT:  leaq    (%r15,%[[REG]]), %[[REG]]
; CHECK-ASM-NEXT:  jmpq    *%[[REG]]
