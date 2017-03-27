; Test that CALL64pcrel32 is aligned to the end of its bundle.

; RUN: llc -print-after-all < %s 2>&1 | FileCheck -check-prefix=CHECK-IR %s
; RUN: llc -filetype obj -verify-machineinstrs < %s | llvm-objdump -d - | FileCheck -check-prefix=CHECK-ASM %s

target triple = "x86_64-hadean-linux"

define i64 @foo() {
  ret i64 12345
}

define void @test() {
  call i64 @foo()
  ret void
}

; CHECK-IR:        CALL64pcrel32

; CHECK-ASM:       test:
; CHECK-ASM:       nop{{[w]?}}
; CHECK-ASM:       callq  {{[-0-9]+}} <foo>
; CHECK-ASM-NOT:   nop{{[w]?}}
; CHECK-ASM:       jmpq
