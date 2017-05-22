; Test that RETQ is instrumented with control-flow integrity AND+LEA.

; RUN: llc -print-after-all < %s 2>&1 | FileCheck -check-prefix=CHECK-IR %s
; RUN: llc -filetype obj -verify-machineinstrs < %s | llvm-objdump -d - | FileCheck -check-prefix=CHECK-ASM %s

target triple = "x86_64-hadean-linux"

define i8* @test()  {
  ret i8* null
}

; CHECK-IR:        RET
; CHECK-IR:        RETQ

; CHECK-ASM:       popq    %[[REG:r..]]
; CHECK-ASM-NEXT:  andq    $-32, %[[REG]]
; CHECK-ASM-NEXT:  leaq    (%r15,%[[REG]]), %[[REG]]
; CHECK-ASM-NEXT:  jmpq    *%[[REG]]
