// RUN: llvm-mc -hadean-mpx=true -hadean-mpx-stackptropt=false -assemble -triple=x86_64-hadean-linux -filetype obj < %s | llvm-objdump -d - | FileCheck -check-prefix=CHECK %s
// RUN: llvm-mc -hadean-mpx=true -hadean-mpx-stackptropt=true -assemble -triple=x86_64-hadean-linux -filetype obj < %s | llvm-objdump -d - | FileCheck -check-prefix=CHECK-OPT %s

.text

// ====== PUSH 16-BIT REGISTER ======

pushw %dx

// CHECK:          bndcl  -2(%rsp), %bnd3
// CHECK-NEXT:     bndcu  -1(%rsp), %bnd3
// CHECK-NEXT:     pushw  %dx

// CHECK-OPT:      bndcl  -2(%rsp), %bnd3
// CHECK-OPT-NEXT: pushw  %dx

// ====== PUSH 64-BIT REGISTER ======

pushq %rdx

// CHECK:          bndcl  -8(%rsp), %bnd3
// CHECK-NEXT:     bndcu  -1(%rsp), %bnd3
// CHECK-NEXT:     pushq  %rdx

// CHECK-OPT:      bndcl  -8(%rsp), %bnd3
// CHECK-OPT-NEXT: pushq  %rdx

// ====== POP 16-BIT REGISTER ======

popw %dx

// CHECK:          bndcl  (%rsp), %bnd3
// CHECK-NEXT:     bndcu  1(%rsp), %bnd3
// CHECK-NEXT:     popw  %dx

// CHECK-OPT:      bndcu  1(%rsp), %bnd3
// CHECK-OPT-NEXT: popw   %dx

// ====== POP 64-BIT REGISTER ======

popq %rdx

// CHECK:          bndcl  (%rsp), %bnd3
// CHECK-NEXT:     bndcu  7(%rsp), %bnd3
// CHECK-NEXT:     popq  %rdx

// CHECK-OPT:      bndcu  7(%rsp), %bnd3
// CHECK-OPT-NEXT: popq   %rdx

// ====== SET SPL REGISTER ======

andb $42, %spl

// CHECK:          andb $42, %spl
// CHECK-NOT:      bndcl
// CHECK-NOT:      bndcu

// CHECK-OPT:      andb $42, %spl
// CHECK-OPT-NEXT: bndcl %rsp, %bnd3
// CHECK-OPT-NEXT: bndcu -1(%rsp), %bnd3

// ====== SET SP REGISTER ======

andw $1234, %sp

// CHECK:          andw $1234, %sp
// CHECK-NOT:      bndcl
// CHECK-NOT:      bndcu

// CHECK-OPT:      andw $1234, %sp
// CHECK-OPT-NEXT: bndcl %rsp, %bnd3
// CHECK-OPT-NEXT: bndcu -1(%rsp), %bnd3

// ====== SET ESP REGISTER ======

andl $65000, %esp

// CHECK:          andl $65000, %esp
// CHECK-NOT:      bndcl
// CHECK-NOT:      bndcu

// CHECK-OPT:      andl $65000, %esp
// CHECK-OPT-NEXT: bndcl %rsp, %bnd3
// CHECK-OPT-NEXT: bndcu -1(%rsp), %bnd3

// ====== SET RSP REGISTER ======

andq $65535, %rsp

// CHECK:          andq $65535, %rsp
// CHECK-NOT:      bndcl
// CHECK-NOT:      bndcu

// CHECK-OPT:      andq $65535, %rsp
// CHECK-OPT-NEXT: bndcl %rsp, %bnd3
// CHECK-OPT-NEXT: bndcu -1(%rsp), %bnd3

// ====== POP INTO RSP REGISTER ======

popq %rsp

// CHECK:          bndcl  (%rsp), %bnd3
// CHECK-NEXT:     bndcu  7(%rsp), %bnd3
// CHECK:          popq %rsp
// CHECK-NOT:      bndcl
// CHECK-NOT:      bndcu

// CHECK-OPT:      bndcu 7(%rsp), %bnd3
// CHECK-OPT-NEXT: popq %rsp
// CHECK-OPT-NEXT: bndcl %rsp, %bnd3
// CHECK-OPT-NEXT: bndcu -1(%rsp), %bnd3
