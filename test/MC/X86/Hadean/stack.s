// RUN: llvm-mc -hadean-mpx=true -assemble -triple=x86_64-hadean-linux -filetype obj < %s | llvm-objdump -d - | FileCheck %s

.text

// ====== PUSH 16-BIT REGISTER ======

pushw %dx

// CHECK:      bndcl  -2(%rsp), %bnd3
// CHECK-NEXT: pushw  %dx

// ====== PUSH 64-BIT REGISTER ======

pushq %rdx

// CHECK:      bndcl  -8(%rsp), %bnd3
// CHECK-NEXT: pushq  %rdx

// ====== POP 16-BIT REGISTER ======

popw %dx

// CHECK:      bndcu  2(%rsp), %bnd3
// CHECK-NEXT: popw   %dx

// ====== POP 64-BIT REGISTER ======

popq %rdx

// CHECK:      bndcu  8(%rsp), %bnd3
// CHECK-NEXT: popq   %rdx

// ====== SET SPL REGISTER ======

andb $42, %spl

// CHECK:      andb $42, %spl
// CHECK-NEXT: bndcl %rsp, %bnd3
// CHECK-NEXT: bndcu %rsp, %bnd3

// ====== SET SP REGISTER ======

andw $1234, %sp

// CHECK:      andw $1234, %sp
// CHECK-NEXT: bndcl %rsp, %bnd3
// CHECK-NEXT: bndcu %rsp, %bnd3

// ====== SET ESP REGISTER ======

andl $65000, %esp

// CHECK:      andl $65000, %esp
// CHECK-NEXT: bndcl %rsp, %bnd3
// CHECK-NEXT: bndcu %rsp, %bnd3

// ====== SET RSP REGISTER ======

andq $65535, %rsp

// CHECK:      andq $65535, %rsp
// CHECK-NEXT: bndcl %rsp, %bnd3
// CHECK-NEXT: bndcu %rsp, %bnd3

// ====== POP INTO RSP REGISTER ======

popq %rsp

// CHECK:      bndcu 8(%rsp), %bnd3
// CHECK-NEXT: popq %rsp
// CHECK-NEXT: bndcl %rsp, %bnd3
// CHECK-NEXT: bndcu %rsp, %bnd3
