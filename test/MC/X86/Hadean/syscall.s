// RUN: llvm-mc -assemble -hadean-debug-cfi=false -triple=x86_64-hadean-linux -filetype obj < %s | llvm-objdump -d - | FileCheck %s

.section .text
  mov $60, %rax
  syscall
  hlt
