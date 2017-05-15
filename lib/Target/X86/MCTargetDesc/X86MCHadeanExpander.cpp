#include "MCTargetDesc/X86MCHadeanExpander.h"

#include <cstdio>
#include <cassert>

#include "llvm/Support/CommandLine.h"
#include <llvm/MC/MCInst.h>
#include <llvm/MC/MCExpr.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCInstBuilder.h>
#include <llvm/MC/MCStreamer.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include "MCTargetDesc/X86MCTargetDesc.h"

namespace llvm {

// To enable, pass '-mllvm --hadean-debug-cfi' to Clang.
// TODO: This is enaled by default for now. Disable before putting into production!.
cl::opt<bool> DebugCFI("hadean-debug-cfi",
                       cl::desc("Debug instrumentation for Hadean CFI"),
                       cl::init(true));

bool HadeanExpander::expandInstruction(MCStreamer &out, const MCInst &inst) {
  // If this is a recursive expansion of `inst`, return immediately.
  if (emitRaw_) {
    return false;
  }

  switch (inst.getOpcode()) {
    case X86::CALL64pcrel32: EmitDirectCall(out, inst);   break;
    case X86::CALL64r:       EmitIndirectCall(out, inst); break;
    case X86::HAD_JMP64r:
    case X86::TAILJMPr64:    EmitJump(out, inst);         break;
    case X86::RETQ:          EmitReturn(out, inst);       break;

    case X86::JMP64r:
    case X86::JMP64m:
      report_fatal_error("Found indirect 'jmp' instrution. Use 'had_jmp' instead");

    case X86::CALL64m:
    case X86::HAD_JMP64m:
      report_fatal_error("Instruction should have been expanded");

    default:
      return false;
  }

  return true;
}

void HadeanExpander::EmitJump(MCStreamer &out, const MCInst& inst) {
  assert(inst.getOpcode() == X86::HAD_JMP64r || inst.getOpcode() == X86::TAILJMPr64);
  assert(inst.getNumOperands() == 1);
  assert(inst.getOperand(0).isReg());
  EmitSafeBranch(
      out, inst, /* opcode */ X86::JMP64r, inst.getOperand(0).getReg(), /* alignToEnd */ false);
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
  EmitSafeBranch(
      out, inst, /* opcode */ X86::CALL64r, inst.getOperand(0).getReg(), /* alignToEnd */ true);
}

void HadeanExpander::EmitReturn(MCStreamer &out, const MCInst &inst) {
  assert(inst.getOpcode() == X86::RETQ);
  static constexpr unsigned kTempReg = X86::R11;  // register not preserved across function calls

  MCInstBuilder instPOP(X86::POP64r);
  instPOP.addReg(kTempReg);
  out.EmitInstruction(instPOP, STI_);

  MCInstBuilder instJMP(X86::JMP64r);
  instJMP.addReg(kTempReg);
  EmitSafeBranch(out, instJMP, /* opcode */ X86::JMP64r, kTempReg, /* alignToEnd */ false);
}

void HadeanExpander::EmitSafeBranch(MCStreamer &out,
                                    const MCInst &inst,
                                    unsigned opcode,
                                    unsigned targetReg,
                                    bool alignToEnd) {
  if (DebugCFI) {
    // Do an AND $31 on `targetReg`. Trap if the bottom five bits are not zero.
    MCContext &context = out.getContext();
    MCSymbol *labelFine = context.createTempSymbol();
    out.EmitInstruction(MCInstBuilder(X86::PUSH64r).addReg(targetReg), STI_);
    out.EmitInstruction(MCInstBuilder(X86::AND64ri32).addReg(targetReg).addReg(targetReg).addImm(31), STI_);
    out.EmitInstruction(MCInstBuilder(X86::JE_4).addExpr(MCSymbolRefExpr::create(labelFine, context)), STI_);
    out.EmitInstruction(MCInstBuilder(X86::TRAP), STI_);
    out.EmitLabel(labelFine);
    out.EmitInstruction(MCInstBuilder(X86::POP64r).addReg(targetReg), STI_);
  }

  out.EmitBundleLock(alignToEnd);

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

  // Emit the branch instruction
  MCInstBuilder instBranch(opcode);
  instBranch.addReg(targetReg);
  emitRaw_ = true; out.EmitInstruction(instBranch, STI_); emitRaw_ = false;

  out.EmitBundleUnlock();
}

}  // namespace llvm
