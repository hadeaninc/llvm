// RUN: llvm-mc --hadean-cfi=false -assemble -triple=x86_64-hadean-linux -filetype obj < %s | llvm-objdump -d - | FileCheck %s

// Test that hadjmp is replaced with jmp when CFI disabled

.text
  hadjmp *%rax

// CHECK-NOT: hadjmp
// CHECK:     jmpq *%rax
