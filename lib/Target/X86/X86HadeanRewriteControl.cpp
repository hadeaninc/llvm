#include <iterator>
#include <cassert>
#include "X86.h"
#include "X86InstrBuilder.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/MC/MCContext.h"
using namespace llvm;

namespace {

class X86HadeanRewriteControl : public MachineFunctionPass {
public:
  static char ID;

  X86HadeanRewriteControl() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &fuction) override;
  const char *getPassName() const override;

private:
  bool rewriteMI(MachineBasicBlock &MBB, MachineInstr &MI);
  bool rewriteMBB(MachineBasicBlock &MBB);
};

char X86HadeanRewriteControl::ID = 0;

}  // anonymous namespace

const char *X86HadeanRewriteControl::getPassName() const {
    return "Hadean control flow instruction rewriter";
}

bool X86HadeanRewriteControl::runOnMachineFunction(MachineFunction &MF) {
  bool modified = false;
  for (MachineBasicBlock &MBB : MF)
    modified |= rewriteMBB(MBB);

  return modified;
}

bool X86HadeanRewriteControl::rewriteMBB(MachineBasicBlock &MBB) {
  // TODO: Do only for taken MBBs.
  MBB.setAlignment(5);

  bool modified = false;
  // MBBI may be invalidated by the expansion.
  MachineBasicBlock::iterator MBBIter = MBB.begin(), MBBEnd = MBB.end();
  while (MBBIter != MBBEnd) {
    const MachineBasicBlock::iterator MBBIterNext = std::next(MBBIter);
    modified |= rewriteMI(MBB, *MBBIter);
    MBBIter = MBBIterNext;
  }

  return modified;
}

bool X86HadeanRewriteControl::rewriteMI(MachineBasicBlock &MBB, MachineInstr &MI) {
  // Assume that first 5 operands are in memory addressing format.
  static constexpr unsigned kNumMemOperands = 5;

  unsigned newOpcode = 0;
  const TargetRegisterClass *regClass = nullptr;

  switch (MI.getOpcode()) {
   case X86::CALL64m:      newOpcode = X86::CALL64r;      regClass = &X86::GR64RegClass;    break;
   case X86::JMP64m:       newOpcode = X86::JMP64r;       regClass = &X86::GR64RegClass;    break;
   case X86::TCRETURNmi64: newOpcode = X86::TCRETURNri64; regClass = &X86::GR64_TCRegClass; break;
   default:                return false;
  }

  MachineFunction &MF = *MBB.getParent();
  const X86Subtarget &STI = static_cast<const X86Subtarget &>(MF.getSubtarget());
  const X86InstrInfo &TII = *STI.getInstrInfo();
  const DebugLoc dl = MI.getDebugLoc();

  // Create a new virtual register
  unsigned vreg = MBB.getParent()->getRegInfo().createVirtualRegister(regClass);

  // Move the target address to `vreg`
  auto instMOV = BuildMI(MBB, &MI, dl, TII.get(X86::MOV64rm));
  instMOV.addReg(vreg, RegState::Define);
  for (unsigned i = 0; i < kNumMemOperands; ++i) {
    // Copy mem access operands to the MOV.
    instMOV.addOperand(MI.getOperand(i));
  }

  // Emit a register branch to the target in `vreg`.
  auto instJMP = BuildMI(MBB, &MI, dl, TII.get(newOpcode));
  instJMP.addReg(vreg, RegState::Kill);
  for (unsigned i = kNumMemOperands; i < MI.getNumOperands(); ++i) {
    // Copy "other" operands to the new instruction. This includes
    // RET stack adjustment for TCRETURN.
    instJMP.addOperand(MI.getOperand(i));
  }

  MI.eraseFromParent();
  return true;
}

FunctionPass *llvm::createX86HadeanRewriteControl() {
  return new X86HadeanRewriteControl();
}
