#include <iterator>
#include <cassert>
#include "X86.h"
//#include "X86FrameLowering.h"
#include "X86InstrBuilder.h"
//#include "X86InstrInfo.h"
//#include "X86MachineFunctionInfo.h"
#include "X86Subtarget.h"
//#include "llvm/Analysis/EHPersonalities.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/MC/MCContext.h"
//#include "llvm/CodeGen/Passes.h" // For IDs of passes that are preserved.
//#include "llvm/IR/GlobalValue.h"
using namespace llvm;

namespace {

class X86HadeanRewriteControl : public MachineFunctionPass {
public:
  static char ID;

  X86HadeanRewriteControl() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &fuction) override;
  const char *getPassName() const override;

private:
  static bool HasControlFlow(const MachineInstr &MI);
  static bool IsDirectBranch(const MachineInstr &MI);

  bool rewriteMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBIter);
  bool rewriteMBB(MachineBasicBlock &MBB);
};

char X86HadeanRewriteControl::ID = 0;

}

const char *X86HadeanRewriteControl::getPassName() const {
    return "Hadean Control Flow Instruction Rewriter";
}

bool X86HadeanRewriteControl::HasControlFlow(const MachineInstr &MI) {
 return MI.getDesc().isBranch() ||
        MI.getDesc().isCall() ||
        MI.getDesc().isReturn() ||
        MI.getDesc().isTerminator() ||
        MI.getDesc().isBarrier();
}

bool X86HadeanRewriteControl::IsDirectBranch(const MachineInstr &MI) {
  return  MI.getDesc().isBranch() &&
         !MI.getDesc().isIndirectBranch();
}


bool X86HadeanRewriteControl::runOnMachineFunction(MachineFunction &MF) {
  printf("Hadean pass running!\n");

  bool modified = false;
  for (MachineBasicBlock &MBB : MF)
    modified |= rewriteMBB(MBB);

  return modified;
}

bool X86HadeanRewriteControl::rewriteMBB(MachineBasicBlock &MBB) {
  bool modified = false;
  // MBBI may be invalidated by the expansion.
  MachineBasicBlock::iterator MBBIter = MBB.begin(), MBBEnd = MBB.end();
  while (MBBIter != MBBEnd) {
    const MachineBasicBlock::iterator MBBIterNext = std::next(MBBIter);
    modified |= rewriteMI(MBB, MBBIter);
    MBBIter = MBBIterNext;
  }

  return modified;
}

bool X86HadeanRewriteControl::rewriteMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBIter) {
  MachineInstr &MI = *MBBIter;

  if (!HasControlFlow(MI))
    return false;

  if (IsDirectBranch(MI))
    return false;

  MachineFunction &MF = *MBB.getParent();
  const X86Subtarget &STI = static_cast<const X86Subtarget &>(MF.getSubtarget());
  const X86InstrInfo &TII = *STI.getInstrInfo();
  const DebugLoc dl = MI.getDebugLoc();
  const unsigned opcode = MI.getOpcode();
  unsigned newOpcode = 0;

  switch (opcode) {
    case X86::CALL64pcrel32:  return false;
    case X86::CALL64r:        newOpcode = X86::HAD_CALL64r; break;
    case X86::JMP64r:         newOpcode = X86::HAD_JMP64r;  break;
    case X86::TAILJMPr64:     newOpcode = X86::HAD_JMP64r;  break;
    case X86::TAILJMPr64_REX: newOpcode = X86::HAD_JMP64r;  break;
    default: break;
  }

  if (newOpcode != 0) {
    const unsigned targetReg = MI.getOperand(0).getReg();
    BuildMI(MBB, MBBIter, dl, TII.get(newOpcode)).addReg(targetReg);
    MI.eraseFromParent();
    return true;
  }

  switch (opcode) {
    case X86::RETQ: newOpcode = X86::HAD_RET64;   break;
    default: break;
  }

  if (newOpcode != 0) {
    BuildMI(MBB, MBBIter, dl, TII.get(newOpcode));
    MI.eraseFromParent();
    return true;
  }

  MI.dump();
  report_fatal_error("Unhandled control-flow instruction in Hadean pass.");
  return false;
}

FunctionPass *llvm::createX86HadeanRewriteControl() {
  return new X86HadeanRewriteControl();
}
