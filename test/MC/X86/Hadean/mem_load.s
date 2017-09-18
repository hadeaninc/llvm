// RUN: llvm-mc -hadean-mpx=true -assemble -mcpu=knl -triple=x86_64-hadean-linux -filetype obj < %s | llvm-objdump -d - | FileCheck %s

.text

// ====== 8-BIT VALUE ======

andb 44(%rdx), %al

// CHECK:      bndcl  44(%rdx), %bnd2
// CHECK-NEXT: bndcu  44(%rdx), %bnd2
// CHECK-NEXT: andb   44(%rdx), %al

// ====== 16-BIT VALUE ======

andw -1234(%rbx,%rcx), %ax

// CHECK:      bndcl  -1234(%rbx,%rcx), %bnd2
// CHECK-NEXT: bndcu  -1233(%rbx,%rcx), %bnd2
// CHECK-NEXT: andw   -1234(%rbx,%rcx), %ax

// ====== 32-BIT VALUE ======

andl -1234(%rbx,%rcx,2), %eax

// CHECK:      bndcl  -1234(%rbx,%rcx,2), %bnd2
// CHECK-NEXT: bndcu  -1231(%rbx,%rcx,2), %bnd2
// CHECK-NEXT: andl   -1234(%rbx,%rcx,2), %eax

// ====== 32-BIT FLOAT VALUE ======

movss (%rax), %xmm0

// CHECK:      bndcl  (%rax), %bnd2
// CHECK-NEXT: bndcu  3(%rax), %bnd2
// CHECK-NEXT: movss (%rax), %xmm0

// ====== 64-BIT VALUE ======

andq -1234(%rbx,%rcx,4), %rax

// CHECK:      bndcl  -1234(%rbx,%rcx,4), %bnd2
// CHECK-NEXT: bndcu  -1227(%rbx,%rcx,4), %bnd2
// CHECK-NEXT: andq   -1234(%rbx,%rcx,4), %rax

// ====== 64-BIT FLOAT VALUE ======

movsd 32(%rcx), %xmm1

// CHECK:      bndcl  32(%rcx), %bnd2
// CHECK-NEXT: bndcu  39(%rcx), %bnd2
// CHECK-NEXT: movsd  32(%rcx), %xmm1

// ====== 80-BIT FLOAT VALUE ======

fldt 64(%rax)

// CHECK:      bndcl  64(%rax), %bnd2
// CHECK-NEXT: bndcu  73(%rax), %bnd2
// CHECK-NEXT: fldt   64(%rax)

// ====== 128-BIT FLOAT VALUE ======

movaps 256(%rbx), %xmm0

// CHECK:      bndcl  256(%rbx), %bnd2
// CHECK-NEXT: bndcu  271(%rbx), %bnd2
// CHECK-NEXT: movaps 256(%rbx), %xmm0

// ====== 512-BIT VALUE ======

vmovdqa64 16(%rdx), %zmm19

// CHECK:      bndcl  16(%rdx), %bnd2
// CHECK-NEXT: bndcu  79(%rdx), %bnd2
// CHECK-NEXT: vmovdqa64 16(%rdx), %zmm19
