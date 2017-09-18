// RUN: llvm-mc -hadean-mpx=true -assemble -mcpu=knl -triple=x86_64-hadean-linux -filetype obj < %s | llvm-objdump -d - | FileCheck %s

.text

// ====== 8-BIT VALUE ======

andb %al, 44(%rdx)

// CHECK:      bndcl  44(%rdx), %bnd3
// CHECK-NEXT: bndcu  44(%rdx), %bnd3
// CHECK-NEXT: andb   %al, 44(%rdx)

// ====== 16-BIT VALUE ======

andw %ax, -1234(%rbx,%rcx)

// CHECK:      bndcl  -1234(%rbx,%rcx), %bnd3
// CHECK-NEXT: bndcu  -1233(%rbx,%rcx), %bnd3
// CHECK-NEXT: andw   %ax, -1234(%rbx,%rcx)

// ====== 32-BIT VALUE ======

andl %eax, -1234(%rbx,%rcx,2)

// CHECK:      bndcl  -1234(%rbx,%rcx,2), %bnd3
// CHECK-NEXT: bndcu  -1231(%rbx,%rcx,2), %bnd3
// CHECK-NEXT: andl   %eax, -1234(%rbx,%rcx,2)

// ====== 32-BIT FLOAT VALUE ======

movss %xmm0, (%rax)

// CHECK:      bndcl  (%rax), %bnd3
// CHECK-NEXT: bndcu  3(%rax), %bnd3
// CHECK-NEXT: movss %xmm0, (%rax)

// ====== 64-BIT VALUE ======

andq %rax, -1234(%rbx,%rcx,4)

// CHECK:      bndcl  -1234(%rbx,%rcx,4), %bnd3
// CHECK-NEXT: bndcu  -1227(%rbx,%rcx,4), %bnd3
// CHECK-NEXT: andq   %rax, -1234(%rbx,%rcx,4)

// ====== 64-BIT FLOAT VALUE ======

movsd %xmm1, 32(%rcx)

// CHECK:      bndcl  32(%rcx), %bnd3
// CHECK-NEXT: bndcu  39(%rcx), %bnd3
// CHECK-NEXT: movsd  %xmm1, 32(%rcx)

// ====== 80-BIT FLOAT VALUE ======

fstpt 64(%rax)

// CHECK:      bndcl  64(%rax), %bnd3
// CHECK-NEXT: bndcu  73(%rax), %bnd3
// CHECK-NEXT: fstpt  64(%rax)

// ====== 128-BIT FLOAT VALUE ======

movaps %xmm0, 256(%rbx)

// CHECK:      bndcl  256(%rbx), %bnd3
// CHECK-NEXT: bndcu  271(%rbx), %bnd3
// CHECK-NEXT: movaps %xmm0, 256(%rbx)

// ====== 512-BIT VALUE ======

vmovdqa64 %zmm19, 16(%rdx)

// CHECK:      bndcl  16(%rdx), %bnd3
// CHECK-NEXT: bndcu  79(%rdx), %bnd3
// CHECK-NEXT: vmovdqa64 %zmm19, 16(%rdx)
