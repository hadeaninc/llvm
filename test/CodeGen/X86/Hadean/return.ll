; Test that RETQ is instrumented with control-flow integrity AND+LEA.

; RUN: llc -hadean-debug-cfi=false -print-after-all < %s 2>&1 | FileCheck -check-prefix=CHECK-IR %s
; RUN: llc -hadean-debug-cfi=true -print-after-all < %s 2>&1 | FileCheck -check-prefix=CHECK-IR %s
; RUN: llc -hadean-debug-cfi=false -filetype obj -verify-machineinstrs < %s | llvm-objdump -d - | FileCheck -check-prefix=CHECK-ASM %s
; RUN: llc -hadean-debug-cfi=true -filetype obj -verify-machineinstrs < %s | llvm-objdump -d - | FileCheck -check-prefix=CHECK-ASM-DBG %s

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

; CHECK-ASM-DBG:       popq    %[[REG:r..]]
; CHECK-ASM-DBG-NEXT:  xorq    %r15, %[[REG]]
; CHECK-ASM-DBG-NEXT:  testq   $31, %[[REG]]
; CHECK-ASM-DBG-NEXT:  je      2
; CHECK-ASM-DBG-NEXT:  ud2
; CHECK-ASM-DBG-NEXT:  xorq    %r15, %[[REG]]
; CHECK-ASM-DBG-NEXT:  jmpq    *%[[REG]]
