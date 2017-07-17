// RUN: llvm-mc -assemble -triple=x86_64-hadean-linux -filetype obj < %s | llvm-objdump -d - | FileCheck %s

.section .data

some_var1:     .long 1234
some_var2:     .long 5678
__hadean_host: .long 0

.section .text

.global __hadean_syscall
.type   __hadean_syscall, @function
__hadean_syscall:
  ret

.global test
.type   test, @function
test:
  mov $60, %rax
  syscall
  hlt
