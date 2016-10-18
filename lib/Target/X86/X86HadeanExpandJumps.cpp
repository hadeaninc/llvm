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

class X86HadeanExpandJumps : public MachineFunctionPass {
public:
  static char ID;

  X86HadeanExpandJumps() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &fuction) override;
  const char *getPassName() const override;

private:
  MachineBasicBlock *dieBlock;

  bool expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBIter);
  bool expandMBB(MachineBasicBlock &MBB);
  void insertAddressValidation(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBIter, unsigned reg);
  unsigned deobfuscateAddress(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBIter, unsigned opaqueTarget);
  MachineBasicBlock *createDieBlock(MachineFunction &MF);
};

char X86HadeanExpandJumps::ID = 0;

}

const char *X86HadeanExpandJumps::getPassName() const {
    return "Hadean Jump Rewriter";
}

bool X86HadeanExpandJumps::runOnMachineFunction(MachineFunction &MF) {
  printf("Hadean pass running!\n");

  dieBlock = createDieBlock(MF);

  bool modified = false;
  for (MachineBasicBlock &MBB : MF)
    modified |= expandMBB(MBB);

  return modified;
}

MachineBasicBlock *X86HadeanExpandJumps::createDieBlock(MachineFunction &MF) {
  const DebugLoc unknown;
  MachineBasicBlock *MBB = MF.CreateMachineBasicBlock();
  MF.insert(MF.end(), MBB);
  MachineBasicBlock::iterator MBBIter = MBB->end();

  const X86Subtarget &STI = static_cast<const X86Subtarget &>(MF.getSubtarget());
  const X86InstrInfo &TII = *STI.getInstrInfo();

  MCContext &context = MF.getContext();
  MCSymbol *const exitFunction = context.getOrCreateSymbol("exit");

  BuildMI(*MBB, MBBIter, unknown, TII.get(X86::MOV64ri)).addReg(X86::EDI).addImm(13);
  BuildMI(*MBB, MBBIter, unknown, TII.get(X86::CALL64pcrel32)).addSym(exitFunction);
  return MBB;
}

bool X86HadeanExpandJumps::expandMBB(MachineBasicBlock &MBB) {
  // MBBI may be invalidated by the expansion.
  MachineBasicBlock::iterator MBBIter = MBB.begin(), MBBEnd = MBB.end();
  while (MBBIter != MBBEnd) {
    const MachineBasicBlock::iterator MBBIterNext = std::next(MBBIter);
    const bool modified = expandMI(MBB, MBBIter);

    if (modified)
      return true;

    MBBIter = MBBIterNext;
  }

  return false;
}

unsigned X86HadeanExpandJumps::deobfuscateAddress(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBIter, const unsigned opaqueTarget) {
  MachineFunction &MF = *MBB.getParent();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const TargetRegisterClass *RegClass = &X86::GR64RegClass;
  const unsigned trueTarget = MRI.createVirtualRegister(RegClass);
  const unsigned maskReg = MRI.createVirtualRegister(RegClass);
  const DebugLoc dl = MBBIter->getDebugLoc();

  const X86Subtarget &STI = static_cast<const X86Subtarget &>(MF.getSubtarget());
  const X86InstrInfo &TII = *STI.getInstrInfo();

  if (false) {
    MCContext &context = MF.getContext();
    MCSymbol *const xorValue = context.getOrCreateSymbol("xor_value");
    BuildMI(MBB, MBBIter, dl, TII.get(X86::MOV64ri)).addReg(maskReg).addSym(xorValue);
  } else {
    BuildMI(MBB, MBBIter, dl, TII.get(X86::MOV64ri)).addReg(maskReg).addImm(0);
  }

  BuildMI(MBB, MBBIter, dl, TII.get(X86::XOR64rr), trueTarget).addReg(opaqueTarget).addReg(maskReg);
  return trueTarget;
}

bool X86HadeanExpandJumps::expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBIter) {
  MachineFunction &MF = *MBB.getParent();
  MachineInstr &MI = *MBBIter;
  const DebugLoc dl = MI.getDebugLoc();
  const X86Subtarget &STI = static_cast<const X86Subtarget &>(MF.getSubtarget());
  const X86InstrInfo &TII = *STI.getInstrInfo();


  if (MI.isCall() || MI.isTerminator()) {
    const unsigned opcode = MI.getOpcode();

    if (MI.isCall()) {
      switch (opcode) {
        case X86::CALL64pcrel32:
          // These are always immediates relative to the IP.
          return false;
        case X86::CALL64m: {
          X86AddressMode addr = getAddressFromInstr(&MI, 0);
          MachineRegisterInfo &MRI = MF.getRegInfo();
          const TargetRegisterClass *RegClass = &X86::GR64RegClass;
          const unsigned opaqueTargetReg = MRI.createVirtualRegister(RegClass);

          addFullAddress(BuildMI(MBB, MI, dl, TII.get(X86::MOV64rm)).addReg(opaqueTargetReg), addr);
          const unsigned trueTargetReg = deobfuscateAddress(MBB, MBBIter, opaqueTargetReg);
          BuildMI(MBB, MBBIter, dl, TII.get(X86::CALL64r)).addReg(trueTargetReg);
          insertAddressValidation(MBB, std::prev(MBBIter), trueTargetReg);
          MI.eraseFromParent();
          return true;
        }
        case X86::CALL64r: {
          const MachineOperand &operand = MI.getOperand(0);
          assert(operand.isReg());
          const unsigned trueTargetReg = deobfuscateAddress(MBB, MBBIter, operand.getReg());
          BuildMI(MBB, MBBIter, dl, TII.get(X86::CALL64r)).addReg(trueTargetReg);
          insertAddressValidation(MBB, std::prev(MBBIter), trueTargetReg);
          MI.eraseFromParent();
          return true;
        }
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

void X86HadeanExpandJumps::insertAddressValidation(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBIter, const unsigned destReg) {
  MachineFunction &MF = *MBB.getParent();
  MCContext &context = MF.getContext();
  MCSymbol *const textStart = context.getOrCreateSymbol("__executable_start");
  MCSymbol *const textEnd = context.getOrCreateSymbol("__etext");

  MachineRegisterInfo &MRI = MF.getRegInfo();
  const TargetRegisterClass *RegClass = &X86::GR64RegClass;
  const DebugLoc dl = MBBIter->getDebugLoc();
  const X86Subtarget &STI = static_cast<const X86Subtarget &>(MF.getSubtarget());
  const X86InstrInfo &TII = *STI.getInstrInfo();

  const unsigned textStartReg = MRI.createVirtualRegister(RegClass);
  const unsigned textEndReg = MRI.createVirtualRegister(RegClass);

  BuildMI(MBB, MBBIter, dl, TII.get(X86::MOV64ri)).addReg(textStartReg).addSym(textStart);
  BuildMI(MBB, MBBIter, dl, TII.get(X86::MOV64ri)).addReg(textEndReg).addSym(textEnd);

  // Lower bound check
  BuildMI(MBB, MBBIter, dl, TII.get(X86::CMP64rr)).addReg(destReg).addReg(textStartReg);
  BuildMI(MBB, MBBIter, dl, TII.get(X86::JL_1)).addMBB(dieBlock);
  MBB.addSuccessorWithoutProb(dieBlock);

  // Upper bound check
  MachineBasicBlock &check2 = *MF.CreateMachineBasicBlock();
  MF.insert(std::next(MBB.getIterator()), &check2);
  BuildMI(check2, check2.end(), dl, TII.get(X86::CMP64rr)).addReg(destReg).addReg(textEndReg);
  BuildMI(check2, check2.end(), dl, TII.get(X86::JGE_1)).addMBB(dieBlock);
  check2.addSuccessorWithoutProb(dieBlock);

  // Splice remainder into new basic block
  MachineBasicBlock &success = *MF.CreateMachineBasicBlock();
  MF.insert(std::next(check2.getIterator()), &success);
  success.splice(success.begin(), &MBB, MBBIter, MBB.end());
  success.transferSuccessorsAndUpdatePHIs(&MBB);
}

FunctionPass *llvm::createX86HadeanExpandJumps() {
  return new X86HadeanExpandJumps();
}
