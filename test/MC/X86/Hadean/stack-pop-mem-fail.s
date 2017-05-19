// RUN: llvm-mc -assemble -triple=x86_64-unknown-linux -filetype obj < %s > /dev/null
// RUN: not llvm-mc -hadean-mpx=true -assemble -triple=x86_64-hadean-linux -filetype obj < %s > /dev/null

.text
popq 16(%rdx)
