// RUN: llvm-mc -assemble -hadean-debug-cfi=false -triple=x86_64-hadean-linux -filetype obj < %s | llvm-objdump -d - | FileCheck %s

.section .text
  mov $60, %rax
  syscall
  hlt

// CHECK:      {{^\.text:}}
// CHECK-NEXT:   movq    $60, %rax
// CHECK-NEXT:   movabsq $0, %rcx
// CHECK-NEXT:   nopl
// CHECK-NEXT:   nopw    -83(%rsi,%rbx,8)
// CHECK-NEXT:   syscall
// CHECK-NEXT:   hlt
