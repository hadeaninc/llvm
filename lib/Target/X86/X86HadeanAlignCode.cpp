#include <iterator>
#include <cassert>
#include "X86.h"
#include "X86InstrBuilder.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/MC/MCContext.h"
using namespace llvm;

namespace {

class X86HadeanAlignCode : public MachineFunctionPass {
public:
  static char ID;

  X86HadeanAlignCode() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &fuction) override;
  const char *getPassName() const override;
};

char X86HadeanAlignCode::ID = 0;

}  // anonymous namespace

const char *X86HadeanAlignCode::getPassName() const {
    return "Hadean code alignment";
}

bool X86HadeanAlignCode::runOnMachineFunction(MachineFunction &MF) {
  // Align function symbol. TODO: Only for taken MFs.
  MF.setAlignment(5);

  // Align basic blocks. TODO: Only for taken MBBs.
  for (MachineBasicBlock &MBB : MF) {
    MBB.setAlignment(5);
  }

  // Align jump table targets.
  MachineJumpTableInfo *JTI = MF.getJumpTableInfo();
  if (JTI != NULL) {
    const std::vector<MachineJumpTableEntry> &JT = JTI->getJumpTables();
    for (unsigned i = 0; i < JT.size(); ++i) {
      const std::vector<MachineBasicBlock*> &MBBs = JT[i].MBBs;
      for (unsigned j = 0; j < MBBs.size(); ++j) {
        MBBs[j]->setAlignment(5);
      }
    }
  }

  return true;
}

FunctionPass *llvm::createX86HadeanAlignCode() {
  return new X86HadeanAlignCode();
}
