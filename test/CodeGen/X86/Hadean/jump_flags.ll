; Test that JMP semantics include clobbering EFLAGS.

; RUN: llc -hadean-debug-cfi=false -O2 -mtriple="x86_64-hadean-linux" -print-after-all < %s 2>&1 | FileCheck -check-prefix=CHECK-HADEAN %s
; RUN: llc -hadean-debug-cfi=false -O2 -mtriple="x86_64-unknown-linux" -print-after-all < %s 2>&1 | FileCheck -check-prefix=CHECK-LINUX %s

declare i32 @bar(i32)

define i32 @test(i32 %arg1, i32 %arg2, i32 %arg3)  {
entry:
  %cond1 = icmp eq i32 %arg1, 11

  ; Dynamically generate a target basic block address. We define it here
  ; rather than pass a pointer to the function to allow control-flow analysis.
  %target = select i1 %cond1, i8* blockaddress(@test, %target1), i8* blockaddress(@test, %target2)

  ; Branch code.
  br i1 %cond1, label %left_branch, label %right_branch

left_branch:
  call i32 @bar(i32 99)                                  ; Side effects to prevent merging branches.

  ; Generate a condition which can be carried to %target2 in EFLAGS.
  %left_cond = icmp eq i32 %arg2, 55

  ; Indirect branch to one of the targets.
  indirectbr i8* %target, [ label %target1, label %target2 ]

right_branch:
  call i32 @bar(i32 88)                                  ; Side effects to prevent merging branches.
  %right_cond = icmp eq i32 %arg3, 77
  br label %target1

target1:
  %phi_cond1 = phi i1 [ %left_cond, %left_branch ], [ %right_cond, %right_branch ]
  %result1 = select i1 %phi_cond1, i32 44, i32 66
  ret i32 %result1

target2:
  ; This branch uses %left_cond defined in %left_branch. If `indirectbr` does not clobber
  ; EFLAGS, LLVM's MachineCSE will avoid moving the flag to a register. On Hadean, this
  ; does not hold and therefore a move is necessary.
  %result2 = select i1 %left_cond, i32 144, i32 166
  ret i32 %result2
}

; Compare the CFG on Linux vs Hadean.

; CHECK-LINUX-LABEL:  IR Dump After StackMap Liveness Analysis
; CHECK-LINUX:        derived from LLVM BB %target2
; CHECK-LINUX-NEXT:   Live Ins: %EFLAGS
; CHECK-LINUX-NEXT:   Predecessors according to CFG

; CHECK-HADEAN-LABEL: IR Dump After StackMap Liveness Analysis
; CHECK-HADEAN:       derived from LLVM BB %target2
; CHECK-HADEAN-NOT:   Live Ins: %EFLAGS
; CHECK-HADEAN:       Predecessors according to CFG
