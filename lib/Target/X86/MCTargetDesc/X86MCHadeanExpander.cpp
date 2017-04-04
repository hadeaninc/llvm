#include "MCTargetDesc/X86MCHadeanExpander.h"

#include <cstdio>
#include <cassert>

#include <llvm/MC/MCInst.h>
#include <llvm/MC/MCExpr.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCInstBuilder.h>
#include <llvm/MC/MCStreamer.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include "MCTargetDesc/X86MCTargetDesc.h"

namespace llvm {

bool HadeanExpander::expandInstruction(MCStreamer &out, const MCInst &inst) {
  // If this is a recursive expansion of `inst`, return immediately.
  if (emitRaw_) {
    return false;
  }

  switch (inst.getOpcode()) {
    default:                 return false;

    case X86::CALL64pcrel32: EmitDirectCall(out, inst);   break;
    case X86::CALL64r:       EmitIndirectCall(out, inst); break;
    case X86::JMP64r:
    case X86::TAILJMPr64:    EmitJump(out, inst);         break;
    case X86::RETQ:          EmitReturn(out, inst);       break;
  }

  return true;
}

void HadeanExpander::EmitJump(MCStreamer &out, const MCInst& inst) {
  assert(inst.getOpcode() == X86::JMP64r || inst.getOpcode() == X86::TAILJMPr64);
  assert(inst.getNumOperands() == 1);
  assert(inst.getOperand(0).isReg());
  EmitSafeBranch(out, inst, inst.getOperand(0).getReg(), /* isCall */ false);
}

void HadeanExpander::EmitDirectCall(MCStreamer &out, const MCInst &inst) {
  assert(inst.getOpcode() == X86::CALL64pcrel32);

  out.EmitBundleLock(/* alignToEnd */ true);
  emitRaw_ = true; out.EmitInstruction(inst, STI_); emitRaw_ = false;
  out.EmitBundleUnlock();
}

void HadeanExpander::EmitIndirectCall(MCStreamer &out, const MCInst &inst) {
  assert(inst.getOpcode() == X86::CALL64r);
  assert(inst.getNumOperands() == 1);
  assert(inst.getOperand(0).isReg());
  EmitSafeBranch(out, inst, inst.getOperand(0).getReg(), /* isCall */ true);
}

void HadeanExpander::EmitReturn(MCStreamer &out, const MCInst &inst) {
  assert(inst.getOpcode() == X86::RETQ);
  static constexpr unsigned kTempReg = X86::R11;  // register not preserved across function calls

  MCInstBuilder instPOP(X86::POP64r);
  instPOP.addReg(kTempReg);
  out.EmitInstruction(instPOP, STI_);

  MCInstBuilder instJMP(X86::JMP64r);
  instJMP.addReg(kTempReg);
  EmitSafeBranch(out, instJMP, kTempReg, /* isCall */ false);
}

void HadeanExpander::EmitSafeBranch(MCStreamer &out,
                                    const MCInst &inst,
                                    unsigned targetReg,
                                    bool isCall) {
  out.EmitBundleLock(/* alignToEnd */ isCall);

  // Mask out bottom bits of the pointer. The immediate will be replaced
  // by elf2hoff to mask out the top bits as well.
  MCInstBuilder instAND(X86::AND64ri32);
  instAND.addReg(targetReg);
  instAND.addReg(targetReg);
  instAND.addImm(-32);
  out.EmitInstruction(instAND, STI_);

  // Add the base from the reserved register.
  MCInstBuilder instLEA(X86::LEA64r);
  instLEA.addReg(targetReg);
  instLEA.addReg(kCodeBaseRegister);
  instLEA.addImm(1);
  instLEA.addReg(targetReg);
  instLEA.addImm(0);
  instLEA.addReg(0);
  out.EmitInstruction(instLEA, STI_);

  // Emit the original instruction
  emitRaw_ = true; out.EmitInstruction(inst, STI_); emitRaw_ = false;

  out.EmitBundleUnlock();
}

}  // namespace llvm
