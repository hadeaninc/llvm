// RUN: llvm-mc -assemble -triple=x86_64-hadean-linux -filetype obj < %s | llvm-objdump -d - | FileCheck %s

// LLVM treats instruction prefixes as separate instructions. We need to make sure
// that our instrumentation does not split the prefix from the instruction it modifies.

.text
  lock
  orl $0, (%rsp)

// CHECK:      bndcl
// CHECK-NEXT: bndcu
// CHECK-NEXT: lock
// CHECK-NEXT: orl
