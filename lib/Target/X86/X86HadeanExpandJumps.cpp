#include <iterator>
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
//#include "llvm/CodeGen/Passes.h" // For IDs of passes that are preserved.
//#include "llvm/IR/GlobalValue.h"
using namespace llvm;

namespace {

class X86HadeanExpandJumps : public MachineFunctionPass {
public:
  static char ID;
  X86HadeanExpandJumps() : MachineFunctionPass(ID) {}

  MachineFunction *MF;
  const X86Subtarget *STI;
  const X86InstrInfo *TII;
  const X86RegisterInfo *TRI;
  //const X86MachineFunctionInfo *X86FI;
  //const X86FrameLowering *X86FL;

  bool runOnMachineFunction(MachineFunction &fuction) override;

  const char *getPassName() const override {
    return "Hadean Jump Rewriter";
  }

private:
  bool expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBIter);
  bool expandMBB(MachineBasicBlock &MBB);
  bool isTargetSafe(MachineOperand &operand);
  bool areTargetsSafe(MachineInstr &instr);
};

char X86HadeanExpandJumps::ID = 0;

}

bool X86HadeanExpandJumps::runOnMachineFunction(MachineFunction &MF) {
  printf("Hadean pass running!\n");
  this->MF = &MF;
  STI = &static_cast<const X86Subtarget &>(MF.getSubtarget());
  TII = STI->getInstrInfo();
  TRI = STI->getRegisterInfo();
  //X86FI = MF.getInfo<X86MachineFunctionInfo>();
  //X86FL = STI->getFrameLowering();

  bool modified = false;
  for (MachineBasicBlock &MBB : MF)
    modified |= expandMBB(MBB);

  return modified;
}

bool X86HadeanExpandJumps::expandMBB(MachineBasicBlock &MBB) {
  bool modified = false;

  // MBBI may be invalidated by the expansion.
  MachineBasicBlock::iterator MBBIter = MBB.begin(), MBBEnd = MBB.end();
  while (MBBIter != MBBEnd) {
    const MachineBasicBlock::iterator MBBIterNext = std::next(MBBIter);
    modified |= expandMI(MBB, MBBIter);
    MBBIter = MBBIterNext;
  }

  return modified;
}

bool X86HadeanExpandJumps::expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBIter) {
  MachineInstr &MI = *MBBIter;

  if (MI.isCall() || MI.isTerminator()) {
    const unsigned opcode = MI.getOpcode();

    if (MI.isCall()) {
      switch (opcode) {
        case X86::CALL64pcrel32:
          // These are always immediates relative to the IP.
          return true;
        case X86::CALL64m: {
          MI.dump();
          X86AddressMode addr = getAddressFromInstr(&MI, 0);
          MachineRegisterInfo &MRI = MF->getRegInfo();
          const TargetRegisterClass *RegClass = &X86::GR64RegClass;
          const unsigned destRegEnc = MRI.createVirtualRegister(RegClass);
          const unsigned destRegDec = MRI.createVirtualRegister(RegClass);
          const unsigned maskReg = MRI.createVirtualRegister(RegClass);
          DebugLoc dl = MI.getDebugLoc();

          addFullAddress(BuildMI(MBB, MI, dl, TII->get(X86::MOV64rm)).addReg(destRegEnc), addr);
          BuildMI(MBB, MI, dl, TII->get(X86::MOV64ri)).addReg(maskReg).addImm(0);
          BuildMI(MBB, MI, dl, TII->get(X86::XOR64rr), destRegDec).addReg(destRegEnc).addReg(maskReg);
          BuildMI(MBB, MI, dl, TII->get(X86::CALL64r)).addReg(destRegDec);

          MI.eraseFromParent();
          return true;
        }
          //report_fatal_error("Hadean: Unhandled unsafe operand to CALLpcrel32");
        default:
          break;
      }
    }

    if (MI.isBranch()) {
      return false;
      //report_fatal_error("A branch!");
    }

    if (MI.isReturn()) {
      return false;
    }

    MI.dump();
    report_fatal_error("Unhandled control-flow instruction in Hadean pass.");
  }

  const bool modified = false;
  return modified;
}

FunctionPass *llvm::createX86HadeanExpandJumps() {
  return new X86HadeanExpandJumps();
}
